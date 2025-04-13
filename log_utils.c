//
// Created by Jhonn on 9/4/2025.
//
#include "log_utils.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static void log_generic(const char* level, const char* format, va_list args) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    printf("[%s] [%s] ", time_str, level);
    vprintf(format, args);
    printf("\n");
}

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_generic("INFO", format, args);
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_generic("ERROR", format, args);
    va_end(args);
}
