#include <stdio.h>
#include <unistd.h>
#include "thread_server.h"
#include "log_utils.h"

void run_client_stress_test();

int main() {
    int option;
    printf("=== Menú de pruebas ===\n");
    printf("1. Iniciar servidor THREAD\n");
    printf("2. Ejecutar cliente de prueba (descarga múltiple)\n");
    printf("Seleccione una opción: ");
    scanf("%d", &option);

    if (option == 1) {
        log_info("Servidor tipo THREAD iniciado desde main");
        run_thread_server();
    } else if (option == 2) {
        pid_t pid = fork();
        if (pid == 0) {
            // Proceso hijo: iniciar el servidor
            log_info("Servidor THREAD iniciado en segundo plano");
            run_thread_server();
            _exit(0);
        } else if (pid > 0) {
            // Proceso padre: esperar un poco y lanzar cliente
            sleep(2);  // Espera para que el servidor esté listo
            run_client_stress_test();

        } else {
            perror("Error al hacer fork()");
        }
    } else {
        printf("Opción inválida.\n");
    }

    return 0;
}
