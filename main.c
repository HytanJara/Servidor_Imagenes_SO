//
// Created by Jhonn on 9/4/2025.
//
#include <stdio.h>
#include "thread_server.h"
#include "log_utils.h"

int main() {
    log_info("Servidor tipo THREAD iniciado desde main");


    run_thread_server();

    log_info("Servidor finalizó (esto no debería pasar)");
    return 0;
}