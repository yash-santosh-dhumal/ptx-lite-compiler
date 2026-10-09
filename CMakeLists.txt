cmake_minimum_required(VERSION 3.14)
project(mini_compiler
    VERSION 1.0.0
    DESCRIPTION "C-Subset Mini-Compiler & Automated Verification Testbed"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mini_compiler/include)

# Source files
set(SOURCES
    mini_compiler/src/lexer.cpp
    mini_compiler/src/parser.cpp
    mini_compiler/src/codegen.cpp
    mini_compiler/src/vm.cpp
    mini_compiler/src/main.cpp
)

# Executable target
add_executable(mini_compiler ${SOURCES})

# Target compiler warnings
if(MSVC)
    target_compile_options(mini_compiler PRIVATE /W4 /permissive-)
else()
    target_compile_options(mini_compiler PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set output directory to project root or build folder for easy locating
set_target_properties(mini_compiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
