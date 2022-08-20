#include "log.h"

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>

log_level g_log_level = ERROR;

char *log_level_to_string(log_level level) {
    switch (level) {
        case NONE:
            return NULL;
        case INFO:
            return "INFO";
        case WARNING:
            return "WARNING";
        case ERROR:
            return "ERROR";
        case DEBUG:
            return "DEBUG";
    }
}

void log_message(log_level level, const char *format, ...) {
    fprintf(stderr, "[%s] ", log_level_to_string(level));

    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}
