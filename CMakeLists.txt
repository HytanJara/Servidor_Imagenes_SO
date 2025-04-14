cmake_minimum_required(VERSION 3.30)
project(Servidor_Imagenes_SO C)

set(CMAKE_C_STANDARD 11)

add_executable(Servidor_Imagenes_SO main.c
        FIFO_server.c
        FIFO_server.h
        log_utils.c
        log_utils.h)
