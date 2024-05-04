cmake_minimum_required(VERSION 3.5)
project(clipsim)
set(SOURCES main.c clipboard.c content.c history.c ipc.c main.c
    send_signal.c util.c)

add_executable(clipsim ${SOURCES})

find_library(MAGIC_LIBRARY magic)
find_package(X11 REQUIRED)
# find_library(XFIXES_LIBRARY libxfixes)
find_library(XFIXES_LIBRARY NAMES Xfixes)

target_link_libraries(clipsim ${MAGIC_LIBRARY} ${X11_LIBRARIES} ${XFIXES_LIBRARY})
