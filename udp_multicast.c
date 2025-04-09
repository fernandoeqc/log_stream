#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "udp_server.h"

#define MULTICAST_IP "239.255.0.1" // Grupo multicast
#define MULTICAST_PORT 5000        // Porta UDP
#define BUFFER_SIZE 128
#define MAX_RECV_LEN 24

static const char *TAG = "UDP_MCAST";

typedef struct
{
    struct sockaddr_in addr;
    char message[BUFFER_SIZE];
} message_t;

static QueueHandle_t message_queue;

int32_t get_multicast_socket()
{
    int sockfd;
    struct sockaddr_in local_addr;
    struct ip_mreq mreq;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        ESP_LOGE(TAG, "Erro ao criar socket");
        printf("Erro ao criar socket\n");
        vTaskDelete(NULL);
    }

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(MULTICAST_PORT);

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        ESP_LOGE(TAG, "Erro no bind");
        perror("Erro no bind\n");
        close(sockfd);
        vTaskDelete(NULL);
    }

    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("Erro ao entrar no grupo multicast\n");
        close(sockfd);
        vTaskDelete(NULL);\
    }

    return sockfd;
}

void udp_multicast_receiver(void *pvParameters)
{
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);
    char recv_buffer[128];

    int32_t sock_multicast = get_multicast_socket();
    if (sock_multicast < 0)
    {
        ESP_LOGE(TAG, "Erro ao criar socket multicast");
        perror("Erro ao criar socket multicast\n");
        vTaskDelete(NULL);
    }

    printf("Aguardando mensagens multicast...\n");

    while (1)
    {
        int len = recvfrom(
            sock_multicast,
            recv_buffer,
            sizeof(recv_buffer) - 1,
            0,
            (struct sockaddr *)&source_addr,
            &addr_len);

        if (len > 0)
        {
            recv_buffer[len] = '\0';
            message_t response;
            response.addr = source_addr;
            printf("Recebido de %s: %s\n", inet_ntoa(source_addr.sin_addr), recv_buffer);
            snprintf(response.message, BUFFER_SIZE, recv_buffer);
            xQueueSend(message_queue, &response, pdMS_TO_TICKS(1000));
        }
    }

    close(sock_multicast);
    vTaskDelete(NULL);
}

void udp_response_sender(void *pvParameters)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    message_t msg;
    bool valid_msg = false;

    while (1)
    {
        xQueueReceive(message_queue, &msg, portMAX_DELAY);

        if (strncmp(msg.message, "get-ip", MAX_RECV_LEN) == 0)
        {
            char ip[16];
            inet_ntoa_r(msg.addr.sin_addr, ip, sizeof(ip));
            snprintf(msg.message, BUFFER_SIZE, "IP: %s", ip);
            valid_msg = true;
        }
        else if (strncmp(msg.message, "enable-stream", MAX_RECV_LEN) == 0)
        {
            start_log_stream(20, 1024);
            snprintf(msg.message, BUFFER_SIZE, "stream-enabled");
            valid_msg = true;
        }
        else if (strncmp(msg.message, "disable-stream", MAX_RECV_LEN) == 0)
        {
            stop_log_stream();
            snprintf(msg.message, BUFFER_SIZE, "stream-disabled");
            valid_msg = true;
        }
        else
        {
            snprintf(msg.message, BUFFER_SIZE, "unknown-command");
            valid_msg = true;
        }

        if (valid_msg)
        {
            valid_msg = false;
            // Response to the sender
            sendto(sockfd, msg.message, strlen(msg.message), 0, (struct sockaddr *)&msg.addr, sizeof(msg.addr));
            printf("Enviado para %s: %s\n", inet_ntoa(msg.addr.sin_addr), msg.message);
        }
    }
}

void init_multicast_listener()
{
    message_queue = xQueueCreate(10, sizeof(message_t));
    if (message_queue == NULL)
    {
        ESP_LOGE(TAG, "Erro ao criar fila de mensagens");
        return;
    }

    xTaskCreate(udp_multicast_receiver, "udp_multicast_receiver", 3*1024, NULL, 5, NULL);
    ESP_LOGI(TAG, "Multicast listening on %s:%d", MULTICAST_IP, MULTICAST_PORT);
}