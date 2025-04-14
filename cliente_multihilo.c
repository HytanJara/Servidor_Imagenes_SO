#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// Dirección IP y puerto 
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 4096

// Estructura para pasar múltiples argumentos a cada hilo de solicitud
typedef struct {
    int id;
    char filename[256];
} ThreadArgs;

// Función que ejecuta cada hilo: solicita un archivo al servidor y lo guarda localmente
void* request_file(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;   // Convertimos el argumento genérico a nuestra estructura

    int sock;                               // Descriptor del socket
    struct sockaddr_in server_addr;        // Dirección del servidor
    char buffer[BUFFER_SIZE];              // Búfer para recibir datos

    // Crear el socket TCP
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creando socket");
        pthread_exit(NULL);
    }

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);                         // Convertir puerto a formato de red
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);              // Convertir IP a binario

    // Conectarse al servidor
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error conectando al servidor");
        close(sock);
        pthread_exit(NULL);
    }

    // Construir solicitud HTTP GET para el archivo deseado
    char request[512];
    snprintf(request, sizeof(request), "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n", args->filename, SERVER_IP);

    // Enviar solicitud al servidor
    send(sock, request, strlen(request), 0);

    // Preparar nombre del archivo de salida, uno por hilo
    char output_name[64];
    snprintf(output_name, sizeof(output_name), "descarga_%d.dat", args->id); // descarga_0.dat

    // Abrir archivo local para guardar la respuesta del servidor
    FILE* output = fopen(output_name, "wb");
    if (!output) {
        perror("Error abriendo archivo de salida");
        close(sock);
        pthread_exit(NULL);
    }

    // Recibir y guardar la respuesta, ignorando los encabezados HTTP
    int received;
    int headers_skipped = 0;

    while ((received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        if (!headers_skipped) {
            // Buscar el final de los encabezados HTTP: "\r\n\r\n"
            char* body = strstr(buffer, "\r\n\r\n");
            if (body) {
                body += 4; // Saltar los 4 caracteres del separador
                fwrite(body, 1, received - (body - buffer), output);
                headers_skipped = 1;
            }
        } else {
            // Una vez omitidos los encabezados, escribir directamente al archivo
            fwrite(buffer, 1, received, output);
        }
    }

    // Cerrar archivo y socket, liberar memoria
    fclose(output);
    close(sock);
    printf("[Hilo %d] Descarga completada: %s\n", args->id, output_name);
    free(arg);
    pthread_exit(NULL);
}

// Función que lanza múltiples hilos de descarga para probar el servidor en simultáneo
void run_client_stress_test() {
    int num_threads;         // Cantidad de hilos que se desean lanzar
    char filename[256];      // Nombre del archivo que se quiere solicitar

    // Obtener entrada del usuario
    printf("Ingrese el nombre del archivo a solicitar (ej: imagen.jpg): ");
    scanf("%255s", filename);

    printf("Ingrese la cantidad de solicitudes simultáneas: ");
    scanf("%d", &num_threads);

    // Arreglo para guardar los identificadores de los hilos
    pthread_t threads[num_threads];

    // Crear hilos
    for (int i = 0; i < num_threads; i++) {
        ThreadArgs* args = malloc(sizeof(ThreadArgs));         // Asignar memoria para los argumentos
        args->id = i;
        strncpy(args->filename, filename, sizeof(args->filename));

        // Lanzar hilo
        if (pthread_create(&threads[i], NULL, request_file, args) != 0) {
            perror("Error creando hilo");
            free(args);
        }
    }

    // Esperar a que todos los hilos terminen
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Todas las descargas finalizaron.\n");
}
