#include "common.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *log_file;

int open_log(const char *path) {
    log_file = fopen(path, "w");
    if (!log_file) { perror(path); return -1; }
    return 0;
}

void log_event(const char *fmt, ...) {
    if (!log_file) return;
    time_t now = time(NULL);
    struct tm tm_now;
    char stamp[32] = "unknown-time";
    if (localtime_r(&now, &tm_now))
        strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", &tm_now);
    fprintf(log_file, "[%s] ", stamp);
    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);
    fputc('\n', log_file);
    fflush(log_file);
}

void close_log(void) {
    if (log_file) fclose(log_file);
    log_file = NULL;
}
