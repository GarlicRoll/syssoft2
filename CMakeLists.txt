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
