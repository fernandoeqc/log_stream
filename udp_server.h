#pragma once
#include <stdint.h>
#include <stddef.h>

void udp_server_task(void *pvParameters);
void init_log_stream(size_t buffer_size, size_t buffer_max_len_inputs);
