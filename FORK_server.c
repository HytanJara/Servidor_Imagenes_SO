//
// Created by Alex Brenes
//

//INCLUDES
#include "FORK_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h> // para el path_max
#include "log_utils.h"
//----------------------------------

//DEFINES
#define FILES_DIR "archivos/"
#define BUFFER_SIZE 8192
#define MAX_CLIENTS 10
#define HTTP_HEADER_SIZE 1024
//----------------------------------

//STATICS
static void handle_client(int client_socket);
static void sigchld_handler(int sig);
static void send_file(int client_socket, const char *filename);
static void send_error(int client_socket, int code, const char *message);
static int validate_path(const char *filepath);
//----------------------------------

//Funcion para correr el server
void run_fork_server(int port) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    pid_t pid;

    //Maneja el Child
    signal(SIGCHLD, sigchld_handler);

    //Cracion del socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_error("Error al crear el socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //Configurar dirección
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    //Enlazar socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
        log_error("Error al enlazar el socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    //Escuchar conexiones
    listen(server_socket, MAX_CLIENTS);
    log_info("[FORK] Escuchando en puerto %d", port);
    log_info("Trabajando con los archivos de la dirección: %s", FILES_DIR);

    while (1) {
        client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EINTR) continue;
            log_error("Error con la conexión: %s", strerror(errno));
            continue;
        }

        log_info("Conexión aceptada %s:%d",
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Manejar cliente en proceso hijo
        if ((pid = fork()) == 0) {
            log_info("Proceso hijo creado con PID %d para manejar al cliente", getpid());
            close(server_socket);
            handle_client(client_socket);
            close(client_socket);
            log_info("Proceso hijo %d finalizado", getpid());
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            log_error("Error en fork(): %s", strerror(errno));
        }
        close(client_socket);
    }
}

static void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        log_error("Error con la solicitud del cliente: %s", strerror(errno));
        return;
    }
    buffer[bytes_read] = '\0';

    //Solicitud HTTP
    char archivo[256];
    sscanf(buffer, "GET /%255s HTTP", archivo);

    log_info("Solicitud recibida: %s", archivo);

    //Llama el FILES_DIR para los archivos
    char ruta[512];
    snprintf(ruta, sizeof(ruta), "%s%s", FILES_DIR, archivo);

    //Validaciones
    if (!validate_path(ruta)) {
        log_error("Acceso denegado (fuera del directorio permitido): %s", ruta);
        send_error(client_socket, 403, "Forbidden");
        return;
    }

    send_file(client_socket, ruta);
}

//Funcion para enviar al archivo y manejo de errores
static void send_file(int client_socket, const char *ruta) {
    struct stat file_stat;
    if (stat(ruta, &file_stat) < 0) {
        log_error("Archivo no encontrado: %s", ruta);
        send_error(client_socket, 404, "Not Found");
        return;
    }

    if (S_ISDIR(file_stat.st_mode)) {
        log_error("Se solicitó un directorio en lugar de un archivo: %s", ruta);
        send_error(client_socket, 400, "Bad Request");
        return;
    }

    int fd = open(ruta, O_RDONLY);
    if (fd < 0) {
        log_error("Error al abrir archivo %s: %s", ruta, strerror(errno));
        send_error(client_socket, 500, "Internal Server Error");
        return;
    }

    //Cabecera del HTTP
    char header[HTTP_HEADER_SIZE];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n",
        file_stat.st_size);

    write(client_socket, header, header_len);
    log_info("Enviando archivo: %s (%ld bytes)", ruta, file_stat.st_size);

    //Enviar archivos mediante BUFFER
    char file_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, file_buffer, BUFFER_SIZE)) > 0) {
        if (write(client_socket, file_buffer, bytes_read) != bytes_read) {
            log_error("Error al enviar archivo al cliente");
            break;
        }
    }
    close(fd);
}

//Funcion para enviar los errores mediantes HTTP
static void send_error(int client_socket, int code, const char *message) {
    char response[HTTP_HEADER_SIZE];
    snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<html><body><h1>%d %s</h1></body></html>\n",
        code, message, code, message);

    write(client_socket, response, strlen(response));
}

//Funcion para validacion de los archivo
static int validate_path(const char *filepath) {
    char real_path[PATH_MAX];
    if (!realpath(filepath, real_path)) return 0;

    char real_files_dir[PATH_MAX];
    if (!realpath(FILES_DIR, real_files_dir)) return 0;

    return strncmp(real_files_dir, real_path, strlen(real_files_dir)) == 0;
}

//Funcion para evitar procesos zombies
static void sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
