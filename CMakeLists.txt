cmake_minimum_required(VERSION 3.3)
project(tiled_video)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/local/include -O2")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib -lSDL2 -lSDL2_image")

set(FASTLZ_FILES
external/fastlz/fastlz.c
external/fastlz/fastlz.h
)

set(COMMON_FILES
src/video.h)

set(CONVERTER_FILES
src/converter.c)

set(PLAYER_FILES
src/player.c)

add_executable(converter ${CONVERTER_FILES} ${COMMON_FILES} ${FASTLZ_FILES})
add_executable(player ${PLAYER_FILES} ${COMMON_FILES} ${FASTLZ_FILES})
