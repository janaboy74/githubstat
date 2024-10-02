cmake_minimum_required(VERSION 3.14)

project(githubstat LANGUAGES CXX)
set(PROJECT_TARGET githubstat)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED "libcurl")

file( GLOB SRCS
    "${PROJECT_SOURCE_DIR}/*.cpp" )

add_executable(${PROJECT_TARGET} ${SRCS})
target_link_libraries(${PROJECT_TARGET} PUBLIC ${CURL_LIBRARIES})
target_include_directories(${PROJECT_TARGET} PUBLIC ${CURL_INCLUDE_DIRS})
