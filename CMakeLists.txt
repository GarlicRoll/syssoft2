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

add_executable(HLVM_LAB2_CLR
        hlvm_lab2/main.c
        hlvm_lab2/source_transformer.c
        hlvm_lab2/source_transformer.h
        hlvm_lab2/clr_model.c
        hlvm_lab2/clr_model.h
        hlvm_lab2/clr_serializer.c
        hlvm_lab2/clr_serializer.h
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

target_include_directories(HLVM_LAB2_CLR PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/hlvm_lab2)

find_package(PkgConfig)
if (PkgConfig_FOUND)
    pkg_check_modules(MONO2 mono-2)
    if (MONO2_FOUND)
        add_library(HLVM_LAB4_BRIDGE SHARED
                hlvm_lab4/php_vm_bridge.c
                hlvm_lab4/php_vm_bridge.h)

        target_include_directories(HLVM_LAB4_BRIDGE PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/hlvm_lab4
                ${MONO2_INCLUDE_DIRS})

        target_link_directories(HLVM_LAB4_BRIDGE PRIVATE ${MONO2_LIBRARY_DIRS})
        target_link_libraries(HLVM_LAB4_BRIDGE PRIVATE ${MONO2_LIBRARIES})
        target_compile_options(HLVM_LAB4_BRIDGE PRIVATE ${MONO2_CFLAGS_OTHER})
        set_target_properties(HLVM_LAB4_BRIDGE PROPERTIES OUTPUT_NAME "hlvm4_bridge")

    endif ()
endif ()
