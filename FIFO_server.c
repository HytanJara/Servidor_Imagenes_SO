//
// Created by hytanjara.
//

#include <stdio.h>
#include <stdlib.h>
#include "log_utils.h"
#include <string.h>
#include <unistd.h>
#include "FIFO_server.h"
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define FILES_DIR "./archivos/"

void fifo_server(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes = read(client_socket, buffer, sizeof(buffer) - 1);
    buffer[bytes] = '\0';

    // Obtener nombre del archivo desde la solicitud HTTP
    char archivo[256];
    sscanf(buffer, "GET /%255s HTTP", archivo);

    char ruta[128];
    snprintf(ruta, sizeof(ruta), "%s%s", FILES_DIR,archivo);

    FILE *file = fopen(ruta, "rb");
    if (file == NULL) {
        char *not_found = "HTTP/1.1 404 Not Found\r\n\r\nArchivo no encontrado.";
        write(client_socket, not_found, strlen(not_found));
        log_error("Archivo no encontrado: %s", ruta);
    } else {
        // Obtener tamaño del archivo
        struct stat st;
        stat(ruta, &st);
        int file_size = st.st_size;

        // Determinar tipo de contenido
        char *content_type = "application/octet-stream";
        if (strstr(archivo, ".png")) content_type = "image/png";
        else if (strstr(archivo, ".jpg") || strstr(archivo, ".jpeg")) content_type = "image/jpeg";
        else if (strstr(archivo, ".html")) content_type = "text/html";
        else if (strstr(archivo, ".txt")) content_type = "text/plain";

        // Enviar cabecera correcta
        char header[256];
        snprintf(header, sizeof(header),
         "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             content_type, file_size);
        write(client_socket, header, strlen(header));

        // Enviar contenido
        int n;
        while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            write(client_socket, buffer, n);
        }
        fclose(file);
    }

    close(client_socket);
}

void run_fifo_server (int puerto) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // cerrar el puerto inmediatamente
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puerto);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_socket, 10);

    printf("Servidor FIFO esperando en el puerto %d...\n", puerto);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        fifo_server(client_socket);
    }

    close(server_socket);
}
