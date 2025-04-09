#include "thread_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define FILES_DIR "./archivos/"

void* handle_client(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    read(client_socket, buffer, BUFFER_SIZE);

    printf("Solicitud recibida:\n%s\n", buffer);

    char filename[256];
    sscanf(buffer, "GET /%s HTTP", filename);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", FILES_DIR, filename);

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        char* not_found = "HTTP/1.1 404 Not Found\r\n\r\nArchivo no encontrado.";
        write(client_socket, not_found, strlen(not_found));
        close(client_socket);
        pthread_exit(NULL);
    }

    char* header = "HTTP/1.1 200 OK\r\n\r\n";
    write(client_socket, header, strlen(header));

    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, file)) > 0) {
        write(client_socket, file_buffer, bytes_read);
    }

    fclose(file);
    close(client_socket);
    pthread_exit(NULL);
}

void run_thread_server() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);
    printf("Servidor THREAD escuchando en puerto %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);

        pthread_t tid;
        int* pclient = malloc(sizeof(int));
        *pclient = new_socket;

        if (pthread_create(&tid, NULL, handle_client, pclient) != 0) {
            perror("pthread_create");
            close(new_socket);
            free(pclient);
        }

        pthread_detach(tid);
    }

    close(server_fd);
}
