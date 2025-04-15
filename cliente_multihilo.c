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
    char output_name[300];
    snprintf(output_name, sizeof(output_name), "descarga_%d_%s", args->id, args->filename);

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
    printf("[Hilo/Proceso %d] Descarga completada: %s\n", args->id, output_name);
    free(arg);
    pthread_exit(NULL);
}

// Función que lanza múltiples hilos de descarga para probar el servidor en simultáneo
void run_client_stress_test() {
    // Arreglo para los nombres de archivos ingresados por el usuario
    char input[1024];
    char* tokens[100]; // Hasta 100 archivos separados por coma
    int num_archivos = 0; // Cantidad de archivos únicos
    int repeticiones = 0; // Cantidad de solicitudes por archivo

    // Obtener entrada del usuario
    printf("Ingrese el/los nombre(s) del/los archivo(s) a solicitar separados por comas (ej: imagen.jpg,documento.txt): ");
    getchar(); // limpiar buffer si viene de un scanf anterior
    fgets(input, sizeof(input), stdin);

    // Eliminar el salto de línea al final si existe
    input[strcspn(input, "\n")] = 0;

    // Separar los nombres usando la coma como delimitador
    char* token = strtok(input, ",");
    while (token != NULL && num_archivos < 100) {
        // Saltar espacios en blanco al inicio del nombre
        while (*token == ' ') token++;
        tokens[num_archivos++] = token;
        token = strtok(NULL, ",");
    }

    // Pedir cuántas veces se desea solicitar cada archivo
    printf("Ingrese la cantidad de solicitudes por archivo: ");
    scanf("%d", &repeticiones);

    int num_threads = num_archivos * repeticiones; // Cantidad total de hilos que se desean lanzar

    // Arreglo para guardar los identificadores de los hilos
    pthread_t threads[num_threads];

    // Crear hilos
    int hilo_index = 0;
    for (int i = 0; i < num_archivos; i++) {
        for (int j = 0; j < repeticiones; j++) {
            ThreadArgs* args = malloc(sizeof(ThreadArgs));         // Asignar memoria para los argumentos
            args->id = hilo_index;
            strncpy(args->filename, tokens[i], sizeof(args->filename));

            // Lanzar hilo
            if (pthread_create(&threads[hilo_index], NULL, request_file, args) != 0) {
                perror("Error creando hilo");
                free(args);
            }
            hilo_index++;
        }
    }

    // Esperar a que todos los hilos terminen
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Todas las descargas finalizaron.\n");
}
