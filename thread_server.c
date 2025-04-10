#include "thread_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "thread_server.h"
#include "log_utils.h" // Para logging
#include <unistd.h> // para chdir()
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

// Contador de hilos activos y mutex
int active_threads = 0;
pthread_mutex_t thread_count_lock = PTHREAD_MUTEX_INITIALIZER;

void* handle_client(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);

    pthread_mutex_lock(&thread_count_lock);
    active_threads++;
    log_info("Hilo iniciado. Hilos activos: %d", active_threads);
    pthread_mutex_unlock(&thread_count_lock);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_received = read(client_socket, buffer, BUFFER_SIZE);

    if (bytes_received <= 0) {
        log_error("No se recibió información del cliente.");
        close(client_socket);

        pthread_mutex_lock(&thread_count_lock);
        active_threads--;
        log_info("Hilo finalizado (sin datos). Hilos activos: %d", active_threads);
        pthread_mutex_unlock(&thread_count_lock);

        pthread_exit(NULL);
    }

    buffer[bytes_received] = '\0';

    char* clean_request = strtok(buffer, "\r\n");
    log_info("Solicitud recibida: %s", clean_request);

    char filename[256];
    if (sscanf(buffer, "GET /%255s HTTP", filename) != 1) {
        log_error("No se pudo extraer el nombre del archivo.");
        close(client_socket);

        pthread_mutex_lock(&thread_count_lock);
        active_threads--;
        log_info("Hilo finalizado (formato inválido). Hilos activos: %d", active_threads);
        pthread_mutex_unlock(&thread_count_lock);

        pthread_exit(NULL);
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", FILES_DIR, filename);

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        char* not_found = "HTTP/1.1 404 Not Found\r\n\r\nArchivo no encontrado.";
        write(client_socket, not_found, strlen(not_found));
        log_error("Archivo no encontrado: %s", filepath);
        close(client_socket);

        pthread_mutex_lock(&thread_count_lock);
        active_threads--;
        log_info("Hilo finalizado (archivo no encontrado). Hilos activos: %d", active_threads);
        pthread_mutex_unlock(&thread_count_lock);

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

    pthread_mutex_lock(&thread_count_lock);
    active_threads--;
    log_info("Hilo finalizado (transferencia completada). Hilos activos: %d", active_threads);
    pthread_mutex_unlock(&thread_count_lock);

    pthread_exit(NULL);
}

void run_thread_server() {
    int server_fd, new_socket;

    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    chdir("/mnt/c/Users/Jhonn/Documents/GitHub/Servidor_Imagenes_SO");
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    log_info("Servidor THREAD escuchando en puerto %d", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);

        pthread_t tid;
        int* pclient = malloc(sizeof(int));
        *pclient = new_socket;

        if (pthread_create(&tid, NULL, handle_client, pclient) != 0) {
            log_error("Error al crear hilo con pthread_create()");
            close(new_socket);
            free(pclient);
        }

        pthread_detach(tid);
    }

    close(server_fd);
}
