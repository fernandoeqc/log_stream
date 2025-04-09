/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "udp_server.h"

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define SERVERPORT CONFIG_SERVER_PORT
#define TAG "UDP_SERVER"

static char *buffer_stream;
static size_t _bff_length;
static QueueHandle_t buffer_queue;

int32_t get_unicast_socket(struct addrinfo **p)
{
   int sockfd = -1;
   struct addrinfo hints, *servinfo;

   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_INET; // set to AF_INET to use IPv4
   hints.ai_socktype = SOCK_DGRAM;

   if ((getaddrinfo("192.168.0.14", "3333", &hints, &servinfo)) != 0)
   {
      perror("getaddrinfo");
      return -1;
   }

   // loop through all the results and make a socket
   for (*p = servinfo; *p != NULL; *p = (*p)->ai_next)
   {
      if ((sockfd = socket((*p)->ai_family, (*p)->ai_socktype,
                           (*p)->ai_protocol)) == -1)
      {
         perror("talker: socket");
         continue;
      }

      break;
   }

   if (*p == NULL)
   {
      fprintf(stderr, "talker: failed to create socket\n");
      return -1;
   }

   printf("p addr: %p\n", *p);
   printf("p ai_addr: %p\n", (*p)->ai_addr);
   return sockfd;
}

void udp_server_task(void *pvParameters)
{
   struct addrinfo *p = NULL;
   char local_buffer[_bff_length];

   int32_t sock_unicast = get_unicast_socket(&p);
   if (sock_unicast < 0 || p == NULL)
   {
      ESP_LOGE(TAG, "Erro ao criar socket unicast");
      perror("Erro ao criar socket unicast\n");
      printf("p é nulo %s\n\n", (p == NULL ? "sim" : "não"));
      vTaskDelete(NULL);
   }

   printf("p addr: %p\n", p);
   printf("p ai_addr: %p\n", p->ai_addr);

   while (true)
   {
      if (xQueueReceive(buffer_queue, local_buffer, portMAX_DELAY) == pdTRUE)
      {
         if ((sendto(sock_unicast, local_buffer, strlen(local_buffer), 0,
                                p->ai_addr, p->ai_addrlen)) == -1)
         {
            perror("talker: sendto");
         }
      }
   }

   esp_log_set_vprintf(vprintf);
   freeaddrinfo(p);
   close(sock_unicast);

   vTaskDelete(NULL);
}

int stream_output(const char *fmt, va_list list)
{
   int len = vsnprintf(buffer_stream, _bff_length, fmt, list);
   printf("stream_output: %s\n", buffer_stream);
   if (xQueueSend(buffer_queue, buffer_stream, portMAX_DELAY) != pdPASS)
   {
      printf("Queue full\n");
   }
   return len;
}

void start_log_stream(size_t buffer_size, size_t buffer_max_len_inputs)
{
   // Check if the log stream is already initialized
   if ( (buffer_queue != NULL) || (buffer_stream != NULL) )
   {
      ESP_LOGE(TAG, "Log stream already initialized");
      return;
   }

   // Create a queue to hold the log messages
   _bff_length = buffer_max_len_inputs;
   buffer_queue = xQueueCreate(buffer_size, buffer_max_len_inputs);
   if (buffer_queue == NULL)
   {
      ESP_LOGE(TAG, "Failed to create log stream queue");
      return;
   }

   // Allocate memory for the buffer stream
   buffer_stream = malloc(sizeof(char) * _bff_length);
   if (buffer_stream == NULL)
   {
      ESP_LOGE(TAG, "Failed to allocate buffer for log stream");
      return;
   }

   // Create the UDP server task
   xTaskCreate(udp_server_task, "udp_server", 4096, (void *)AF_INET, 5, NULL);
   esp_log_set_vprintf(stream_output);
}

void stop_log_stream()
{
   // Delete the queue and free the buffer
   vQueueDelete(buffer_queue);
   free(buffer_stream);
   buffer_queue = NULL;
   buffer_stream = NULL;

   esp_log_set_vprintf(vprintf);
}