#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "config.h"

int sockfd = -1;
int connected = 0;

void *receive_thread(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';
        printf("\r[Server] %s\n> ", buffer);
        fflush(stdout);
    }
    return NULL;
}

int connect_to_server(const char *ip, int port) {
    struct sockaddr_in serv;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv.sin_addr);

    if (connect(s, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect");
        close(s);
        return -1;
    }

    sockfd = s;
    connected = 1;

    pthread_t recv_tid;
    pthread_create(&recv_tid, NULL, receive_thread, NULL);
    pthread_detach(recv_tid);

    printf("[+] Connected to %s:%d\n", ip, port);
    return 0;
}

void disconnect() {
    if (connected) {
        close(sockfd);
        connected = 0;
        printf("[-] Disconnected.\n");
    }
}

void handle_command(char *input) {
    if (strncmp(input, "/leave", 6) == 0) {
        disconnect();
    } else if (strncmp(input, "/join ", 6) == 0) {
        char *ip = input + 6;
        disconnect();
        connect_to_server(ip, DEFAULT_PORT);
    } else if (strncmp(input, "/fav", 4) == 0) {
        FILE *fp = fopen("fav.txt", "r");
        if (!fp) {
            printf("[-] Could not open fav.txt\n");
            return;
        }
        char ip[64];
        fgets(ip, sizeof(ip), fp);
        ip[strcspn(ip, "\n")] = 0;
        fclose(fp);
        disconnect();
        connect_to_server(ip, DEFAULT_PORT);
    } else {
        printf("[-] Unknown command\n");
    }
}

int main() {
    char input[BUFFER_SIZE];

    printf("Commands: /join <ip>, /leave, /fav\n");

    while (1) {
        printf("> ");
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (input[0] == '/') {
            handle_command(input);
        } else if (connected) {
            send(sockfd, input, strlen(input), 0);
        } else {
            printf("[-] Not connected. Use /join <ip> or /fav.\n");
        }
    }

    disconnect();
    return 0;
}
