/*
 * FristOneRR PanVK: driver-side debug log written to a file.
 *
 * Header-only on purpose (no build-system changes). Every translation unit
 * that includes it opens its own O_APPEND descriptor to the same file, and
 * every line is emitted with a single write(), so lines from different
 * threads, processes and files do not interleave inside a line.
 *
 * The log is ON by default. The first location that can be opened wins:
 *    1. $PANVK_LOG_FILE           (exact file; "off" or "0" disables logging)
 *    2. $PANVK_LOG_DIR/panvk.log
 *    3. /storage/emulated/0/Documents/Winlator/panvk.log
 *    4. $TMPDIR/panvk.log
 *    5. /tmp/panvk.log
 *
 * Other switches:
 *    PANVK_LOG_VERBOSE=1   log every submitted atom and completion event
 *                          (default: only the first 32 atoms, all faults)
 *    PANVK_LOG_STDERR=1    also mirror every line to stderr
 */
#ifndef PAN_DBG_LOG_H
#define PAN_DBG_LOG_H

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct panvk_dbg_state {
   int fd;
   bool verbose;
   bool to_stderr;
   char path[256];
};

static inline struct panvk_dbg_state *
panvk_dbg_get(void)
{
   static struct panvk_dbg_state st = {.fd = -1};
   static int init; /* 0 = not started, 1 = in progress, 2 = ready */

   if (__atomic_load_n(&init, __ATOMIC_ACQUIRE) == 2)
      return &st;

   int expected = 0;
   if (!__atomic_compare_exchange_n(&init, &expected, 1, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
      while (__atomic_load_n(&init, __ATOMIC_ACQUIRE) != 2)
         sched_yield();
      return &st;
   }

   const char *e_file = getenv("PANVK_LOG_FILE");
   const char *e_dir = getenv("PANVK_LOG_DIR");
   const char *e_tmp = getenv("TMPDIR");
   const char *e_verbose = getenv("PANVK_LOG_VERBOSE");
   const char *e_stderr = getenv("PANVK_LOG_STDERR");

   st.verbose = e_verbose && e_verbose[0] == '1';
   st.to_stderr = e_stderr && e_stderr[0] == '1';

   const bool disabled =
      e_file && (strcmp(e_file, "off") == 0 || strcmp(e_file, "0") == 0);

   if (!disabled) {
      char cand[5][256];
      int n = 0;

      if (e_file && e_file[0])
         snprintf(cand[n++], sizeof(cand[0]), "%s", e_file);
      if (e_dir && e_dir[0])
         snprintf(cand[n++], sizeof(cand[0]), "%s/panvk.log", e_dir);
      snprintf(cand[n++], sizeof(cand[0]),
               "/storage/emulated/0/Documents/Winlator/panvk.log");
      if (e_tmp && e_tmp[0])
         snprintf(cand[n++], sizeof(cand[0]), "%s/panvk.log", e_tmp);
      snprintf(cand[n++], sizeof(cand[0]), "/tmp/panvk.log");

      for (int i = 0; i < n; i++) {
         int fd = open(cand[i], O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0666);
         if (fd >= 0) {
            st.fd = fd;
            size_t l = strnlen(cand[i], sizeof(st.path) - 1);
            memcpy(st.path, cand[i], l);
            st.path[l] = '\0';
            break;
         }
      }
   }

   __atomic_store_n(&init, 2, __ATOMIC_RELEASE);
   return &st;
}

static inline bool
panvk_dbg_enabled(void)
{
   struct panvk_dbg_state *st = panvk_dbg_get();
   return st->fd >= 0 || st->to_stderr;
}

static inline bool
panvk_dbg_verbose(void)
{
   return panvk_dbg_get()->verbose;
}

/* Path of the log file, or "" when file logging is off. */
static inline const char *
panvk_dbg_path(void)
{
   return panvk_dbg_get()->path;
}

/* True for the first `limit` calls sharing `counter`, and always in verbose
 * mode. */
static inline bool
panvk_dbg_budget(int *counter, int limit)
{
   int c = __atomic_fetch_add(counter, 1, __ATOMIC_RELAXED);
   return c < limit || panvk_dbg_verbose();
}

static inline void __attribute__((format(printf, 1, 2)))
panvk_dbg_log(const char *fmt, ...)
{
   struct panvk_dbg_state *st = panvk_dbg_get();
   if (st->fd < 0 && !st->to_stderr)
      return;

   char buf[1400];
   struct timespec now;
   struct tm tm;
   clock_gettime(CLOCK_REALTIME, &now);
   localtime_r(&now.tv_sec, &tm);

   int off = snprintf(buf, sizeof(buf), "[%02d:%02d:%02d.%03ld p%d t%ld] ",
                      tm.tm_hour, tm.tm_min, tm.tm_sec, now.tv_nsec / 1000000,
                      (int)getpid(), (long)syscall(SYS_gettid));
   if (off < 0)
      return;
   if (off > (int)sizeof(buf) - 2)
      off = (int)sizeof(buf) - 2;

   va_list ap;
   va_start(ap, fmt);
   int n = vsnprintf(buf + off, sizeof(buf) - off - 1, fmt, ap);
   va_end(ap);
   if (n < 0)
      return;

   size_t len = (size_t)off + (size_t)n;
   if (len > sizeof(buf) - 2)
      len = sizeof(buf) - 2;
   buf[len++] = '\n';

   if (st->fd >= 0) {
      ssize_t r = write(st->fd, buf, len);
      (void)r;
   }
   if (st->to_stderr) {
      ssize_t r = write(2, buf, len);
      (void)r;
   }
}

/* One line: "<tag> [N bytes]: xx xx xx ..." (at most 128 bytes). */
static inline void
panvk_dbg_hex(const char *tag, const void *data, size_t len)
{
   if (!panvk_dbg_enabled())
      return;

   char hex[3 * 128 + 1];
   const uint8_t *p = data;

   if (len > 128)
      len = 128;
   for (size_t i = 0; i < len; i++)
      snprintf(hex + i * 3, 4, "%02x ", p[i]);
   hex[len * 3] = '\0';

   panvk_dbg_log("%s [%zu bytes]: %s", tag, len, hex);
}

#endif /* PAN_DBG_LOG_H */
