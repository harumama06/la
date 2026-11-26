cmake_minimum_required(VERSION 3.21)
project(GameControlCenter VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(GameControlCenter WIN32
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/ProfileStore.h
    src/ProfileStore.cpp
    src/WindowsMonitor.h
    src/WindowsMonitor.cpp
    src/WindowsAudio.h
    src/WindowsAudio.cpp
    src/WindowsDisplay.h
    src/WindowsDisplay.cpp
)

target_link_libraries(GameControlCenter PRIVATE Qt6::Widgets)

if(WIN32)
    target_link_libraries(GameControlCenter PRIVATE
        ole32
        uuid
        propsys
        shell32
        user32
        advapi32
    )
endif()

if(WIN32)
    target_compile_definitions(GameControlCenter PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
