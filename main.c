#include <stdio.h>
#include <unistd.h>
#include "thread_server.h"
#include "FIFO_server.h"
#include "FORK_server.h" //
#include "log_utils.h"

// Declaración del cliente de prueba
void run_client_stress_test();

int main() {
    int option;
    printf("=== Menú de pruebas ===\n");
    printf("1. Iniciar servidor THREAD\n");
    printf("2. Iniciar servidor FIFO\n");
    printf("3. Iniciar servidor FORK\n");
    printf("4. Ejecutar cliente de prueba (descarga múltiple con THREAD)\n");
    printf("5. Ejecutar cliente de prueba (descarga múltiple con FIFO)\n");
    printf("6. Ejecutar cliente de prueba (descarga múltiple con FORK)\n");
    printf("Seleccione una opción: ");
    scanf("%d", &option);

    if (option == 1) {
        log_info("Servidor tipo THREAD iniciado desde main");
        run_thread_server();
    } else if (option == 2) {
        log_info("Servidor tipo FIFO iniciado desde main");
        run_fifo_server(8080);
    } else if (option == 3) {
        log_info("Servidor tipo FORK iniciado desde main");
        run_fork_server(8080); //
    } else if (option == 4 || option == 5 || option == 6) {
        pid_t pid = fork();
        if (pid == 0) {
            // Hijo: iniciar servidor correspondiente
            if (option == 4) {
                log_info("Servidor THREAD iniciado en segundo plano");
                run_thread_server();
            } else if (option == 5) {
                log_info("Servidor FIFO iniciado en segundo plano");
                run_fifo_server(8080);
            } else {
                log_info("Servidor FORK iniciado en segundo plano");
                run_fork_server(8080);
            }
            _exit(0); // salida
        } else if (pid > 0) {
            sleep(3); // Espera para que el servidor arranque
            run_client_stress_test();
        } else {
            perror("Error");
        }
    } else {
        printf("Opción inválida.\n");
    }

    return 0;
}
