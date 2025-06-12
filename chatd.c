#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "config.h"

typedef struct {
    int sockfd;
    struct sockaddr_in addr;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(const char *msg, int exclude_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sockfd != exclude_fd) {
            send(clients[i].sockfd, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *client_handler(void *arg) {
    Client *client = (Client *)arg;
    char buffer[MAX_MSG_LENGTH];
    char msg_with_ip[MAX_MSG_LENGTH + 64];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t len = recv(client->sockfd, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) {
            // Client disconnected
            printf("Client %s disconnected\n", inet_ntoa(client->addr.sin_addr));
            break;
        }
        snprintf(msg_with_ip, sizeof(msg_with_ip), "%s: %s", inet_ntoa(client->addr.sin_addr), buffer);
        printf("%s", msg_with_ip);
        broadcast_message(msg_with_ip, client->sockfd);
    }

    close(client->sockfd);

    pthread_mutex_lock(&clients_mutex);
    // Remove client from list
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sockfd == client->sockfd) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    free(client);
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Chat server started on port %d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        if (client_count >= MAX_CLIENTS) {
            pthread_mutex_unlock(&clients_mutex);
            char *msg = "Server full, try again later.\n";
            send(client_fd, msg, strlen(msg), 0);
            close(client_fd);
            continue;
        }

        Client *new_client = malloc(sizeof(Client));
        new_client->sockfd = client_fd;
        new_client->addr = client_addr;
        clients[client_count++] = *new_client;
        pthread_mutex_unlock(&clients_mutex);

        printf("New client connected: %s\n", inet_ntoa(client_addr.sin_addr));

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, new_client);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
