cmake_minimum_required(VERSION 3.15)

project(NSQL
    VERSION 0.1.0
    DESCRIPTION "NSQL: A simple SQL-like language"
    LANGUAGES C CXX
)

# Set C standard globally
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Set build type to Release by default if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Setting build type to Release as none was specified")
endif()

# Source files for the library
set(NSQL_SOURCES
    src/lexer.c
    src/parser.c
    src/ast_serializer.c
    src/ast_printer.c
    src/error_reporter.c
)

# Build NSQL as a static library
add_library(nsql STATIC ${NSQL_SOURCES})

# Include directories for the library
target_include_directories(nsql PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Compiler flags
if(MSVC)
    target_compile_options(nsql PRIVATE
        $<$<CONFIG:Release>:/O2>
        /W4 /permissive-
    )
else()
    target_compile_options(nsql PRIVATE -Wall -Wextra -pedantic -O3)
endif()

# Build configurations
option(ENABLE_SYNC_RECOVERY "Enable parser error recovery via synchronize()" ON)
if (ENABLE_SYNC_RECOVERY)
    target_compile_definitions(nsql PRIVATE ENABLE_SYNC_RECOVERY=1)
endif()

# Installation
install(TARGETS nsql
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Standard test target
if(TARGET test_runner OR TARGET test_lexer OR TARGET test_parser OR TARGET test_serializer)
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS
            $<$<TARGET_EXISTS:test_runner>:test_runner>
            $<$<TARGET_EXISTS:test_lexer>:test_lexer>
            $<$<TARGET_EXISTS:test_parser>:test_parser>
            $<$<TARGET_EXISTS:test_serializer>:test_serializer>
        COMMENT "Running all tests"
    )
endif()
