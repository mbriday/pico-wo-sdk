#to compile the project:
# mkdir _build
# cd _build
# cmake -D CMAKE_TOOLCHAIN_FILE=../../sys/cmake/arm-none-eabi.cmake ..
# make
# make flash
cmake_minimum_required(VERSION 3.5)

#project name
project(blink VERSION 1.0)

#application sources
set(SRCS
        ${CMAKE_SOURCE_DIR}/main.c
)

include("../sys/cmake/pico.cmake")
