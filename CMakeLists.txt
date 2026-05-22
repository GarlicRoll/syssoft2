cmake_minimum_required(VERSION 3.21.1)
project(SYSSOFT C)
set(CMAKE_C_STANDARD 99)


add_executable(SYSSOFT main.c
        node.c
        node.h
        error.c
        error.h
        main.h
        parser.tab.c
        parser.tab.h
        lex.yy.c

        parser.c parser.h
        cfg.c
        cfg.h
        listing.c
        listing.h)

add_executable(HLVM_LAB1_CLR
        hlvm_lab1/main.c
        hlvm_lab1/clr_model.c
        hlvm_lab1/clr_model.h
        hlvm_lab1/clr_serializer.c
        hlvm_lab1/clr_serializer.h
        node.c
        node.h
        error.c
        error.h
        parser.tab.c
        parser.tab.h
        lex.yy.c
        parser.c
        parser.h
        cfg.c
        cfg.h
        listing.c
        listing.h)

target_include_directories(HLVM_LAB1_CLR PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/hlvm_lab1)
