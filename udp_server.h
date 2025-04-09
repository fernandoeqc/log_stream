#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>


int stream_output(const char *fmt, va_list list);
void start_log_stream(size_t buffer_size, size_t buffer_max_len_inputs);
void stop_log_stream();
