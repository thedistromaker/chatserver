#include <stdio.h> // Built on Linux (gcc 14) kernel 6.11.0, 11 Jun 25, 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include "config.h" 

/* For configuring the program, using chatconf (our program) -e, localcfg.h is included 
   as we need it to configure the PORT, MAX_CLIENTS, BUFFER_SIZE and other 
   necessary variables. */

int clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast(char *message, int sender_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != 0 && clients[i] != sender_fd) {
            send(clients[i], message, strlen(message), 0); // Here, we broadcast the message to the clients.
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytes_read] = '\0';
        broadcast(buffer, client_fd);
    }

    // Remove client from chat stream.
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == client_fd) {
            clients[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    close(client_fd);
    free(arg);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, MAX_CLIENTS);
    printf("Server on port %d\n", PORT); // Start sequence ends.

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i] == 0) {
                clients[i] = new_socket;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;
        pthread_t t;
        pthread_create(&t, NULL, handle_client, pclient);
        pthread_detach(t);
    }

    return 0;
}
