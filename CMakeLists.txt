cmake_minimum_required(VERSION 3.10)
project(Servidor_Imagenes_SO C)

set(CMAKE_C_STANDARD 11)

add_executable(Servidor_Imagenes_SO
        thread_server.c
        main.c
        log_utils.c)
