#include "thread_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "log_utils.h" // Utilidad personalizada para imprimir logs

#define PORT 8080                    // Puerto en el que el servidor escucha conexiones
#define BUFFER_SIZE 4096            // Tamaño del búfer de lectura y escritura
#define FILES_DIR "./archivos/"     // Carpeta donde están los archivos

// Contador de hilos activos para monitoreo de concurrencia
int active_threads = 0;
// Mutex que protege el contador de hilos activos, mutex evita que 2 hilos usen los mismos recursos
pthread_mutex_t thread_count_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Función que ejecuta cada hilo. Atiende una conexión entrante.
 * Lee la solicitud del cliente, interpreta el archivo solicitado,
 * lo envía si existe o devuelve un 404 si no.
 */
void* handle_client(void* arg) {
    int client_socket = *((int*)arg);  // Extrae el descriptor del socket del cliente
    free(arg);                         // Libera la memoria pasada por el hilo

    // Incrementa el contador de hilos activos
    pthread_mutex_lock(&thread_count_lock);
    active_threads++;
    log_info("Hilo iniciado. Hilos activos: %d", active_threads);
    pthread_mutex_unlock(&thread_count_lock);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE); // Limpia el buffer

    // Lee la solicitud del cliente
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

    buffer[bytes_received] = '\0'; // Asegura que el buffer sea una string válida

    // Extrae la primera línea (la que contiene el GET)
    char* clean_request = strtok(buffer, "\r\n");
    log_info("Solicitud recibida: %s", clean_request);

    // Extrae el nombre del archivo solicitado
    char filename[256];
    if (sscanf(buffer, "GET /%255s HTTP", filename) != 1) {
        // Si la solicitud está malformada
        log_error("No se pudo extraer el nombre del archivo.");
        close(client_socket);

        pthread_mutex_lock(&thread_count_lock);
        active_threads--;
        log_info("Hilo finalizado (formato inválido). Hilos activos: %d", active_threads);
        pthread_mutex_unlock(&thread_count_lock);

        pthread_exit(NULL);
    }

    // Construye la ruta completa del archivo solicitado
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", FILES_DIR, filename);

    // Intenta abrir el archivo
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        // Archivo no encontrado, enviar mensaje 404
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

    // Enviar encabezado HTTP 200 OK
    char* header = "HTTP/1.1 200 OK\r\n\r\n";
    write(client_socket, header, strlen(header));

    // Leer el archivo y enviarlo en bloques
    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, file)) > 0) {
        write(client_socket, file_buffer, bytes_read);
    }

    // Cierre de archivo y conexión
    fclose(file);
    close(client_socket);

    // Decrementar contador de hilos
    pthread_mutex_lock(&thread_count_lock);
    active_threads--;
    log_info("Hilo finalizado (transferencia completada). Hilos activos: %d", active_threads);
    pthread_mutex_unlock(&thread_count_lock);

    pthread_exit(NULL); // Finaliza el hilo correctamente
}

/**
 * Función principal del servidor THREAD.
 * Configura el socket, escucha en el puerto, y crea un nuevo hilo por cada conexión.
 */
void run_thread_server() {
    int server_fd, new_socket;  // server_fd es el file descriptor del socket que acepta las solicitudes entrantes
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // Asegura que esté en la carpeta raíz del proyecto para encontrar archivos
    chdir("/mnt/c/Users/Jhonn/Documents/GitHub/Servidor_Imagenes_SO");

    // Crea el socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Permite reutilizar el puerto rápidamente después de cerrar
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configura dirección del servidor
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Acepta conexiones en cualquier IP local
    address.sin_port = htons(PORT);       // Puerto a usar

    // Enlaza el socket con la dirección y puerto
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));

    // Escucha con una cola de hasta 100 conexiones
    listen(server_fd, 100);

    log_info("Servidor THREAD escuchando en puerto %d", PORT);

    // Bucle infinito para aceptar y atender conexiones
    while (1) {
        // Espera por una nueva conexión
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);

        // Reserva memoria para pasar el socket al hilo
        pthread_t tid;
        int* pclient = malloc(sizeof(int));
        *pclient = new_socket;

        // Crea un nuevo hilo para atender la conexión
        if (pthread_create(&tid, NULL, handle_client, pclient) != 0) {
            log_error("Error al crear hilo con pthread_create()");
            close(new_socket);
            free(pclient);
        }

        // Se libera el hilo automáticamente al terminar (no se necesita join)
        pthread_detach(tid);
    }

    // Cierra el socket del servidor (nunca se alcanza en práctica)
    close(server_fd);
}
