
########################################################################
## Create RPX-100 project (Implementation of WRAN IEE802.22)
########################################################################
cmake_minimum_required(VERSION 3.22.1)
project("Framer" DESCRIPTION "WRAN RPX-100 Framer" LANGUAGES CXX C)
add_compile_options(-pedantic -Wall)

########################################################################
## compiler flags
########################################################################


## Threads
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

## zlib
set(ZLIB_USE_STATIC_LIBS ON)
find_package(ZLIB REQUIRED)

## C++17 is a required language feature for this project
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
## export compiler settings to json for remote development
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(functions STATIC functions.c functions.h common.h)
target_link_libraries(functions Threads::Threads ZLIB::ZLIB)

add_executable(receiver receiver.c receiver.h common.h)
add_executable(sender sender.c sender.h common.h)
target_link_libraries(receiver Threads::Threads ZLIB::ZLIB functions)
target_link_libraries(sender Threads::Threads ZLIB::ZLIB functions)