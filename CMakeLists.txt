cmake_minimum_required(VERSION 3.14)
project(Practica4IS)

set(CMAKE_CXX_STANDARD 17)

# Fetch Asio (Standalone) - Required by Crow
include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
)
FetchContent_MakeAvailable(asio)

set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)

# Fetch Crow
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(Crow)

# Fetch SQLite
FetchContent_Declare(
    sqlite_amalgamation
    URL https://www.sqlite.org/2023/sqlite-amalgamation-3420000.zip
)
FetchContent_MakeAvailable(sqlite_amalgamation)

include_directories(${sqlite_amalgamation_SOURCE_DIR})
include_directories(${ASIO_INCLUDE_DIR}) # Ensure main target sees it too

# Source files
set(SOURCES
    main.cpp
    src/database.cpp
    ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c
)

add_executable(Practica4IS ${SOURCES})

target_link_libraries(Practica4IS PRIVATE Crow::Crow)
if(WIN32)
    target_link_libraries(Practica4IS PRIVATE ws2_32 mswsock)
endif()
