cmake_minimum_required(VERSION 3.8)
project(pixelsort VERSION 0.1)

find_package(Threads REQUIRED)
find_package(OpenCV REQUIRED)
find_package(SDL2 REQUIRED)

find_package(PkgConfig)
pkg_check_modules(PULSE libpulse-simple)

set(sources
    main.cpp
    pulse_input.cpp
)

set(headers
    pulse_input.h
)

add_executable(pixelsort ${sources} ${headers})
target_include_directories(pixelsort PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${PULSE_INCLUDE_DIRS}
)
target_link_libraries(pixelsort
    ${CMAKE_THREAD_LIBS_INIT}
    ${OpenCV_LIBS}
    ${SDL2_LIBRARIES}
    ${PULSE_LIBRARIES}
)
