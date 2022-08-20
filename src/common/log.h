#ifndef LOG_H
#define LOG_H

typedef enum {
    NONE = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    DEBUG = 4,
} log_level;

extern log_level g_log_level;

void log_message(log_level level, const char *format, ...);

#define LOG(level, ...) if (level > g_log_level) {} else log_message(level, __VA_ARGS__)

#endif
