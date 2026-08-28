cmake_minimum_required(VERSION 3.15)
project(MesaDeDJ CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mesa-dj
    src/main/main.cpp
    src/cpp/Console.cpp
    src/cpp/AudioEngine.cpp
    src/cpp/Instrumento.cpp
    src/cpp/MesaDeDJ.cpp
)

target_include_directories(mesa-dj PRIVATE src/h)

find_package(Threads REQUIRED)
target_link_libraries(mesa-dj PRIVATE Threads::Threads)

# A miniaudio precisa destas bibliotecas no Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(mesa-dj PRIVATE dl m)
endif()

# Copia a pasta de amostras para junto do executavel
add_custom_command(TARGET mesa-dj POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/samples
        $<TARGET_FILE_DIR:mesa-dj>/samples)