cmake_minimum_required(VERSION 3.28)
project(Servidor_Imagenes_SO C)

set(CMAKE_C_STANDARD 11)

add_executable(Servidor_Imagenes_SO main.c
        FIFO_server.c
        FIFO_server.h
        log_utils.c
        thread_server.c
        cliente_multihilo.c
        thread_server.h
        log_utils.h)
