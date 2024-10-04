cmake_minimum_required(VERSION 3.26)
include(FetchContent)

FetchContent_Declare(jsoncpp SYSTEM
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp
    GIT_TAG 89e2973c754a9c02a49974d839779b151e95afd6 # v1.9.6
    GIT_SHALLOW TRUE)
set(JSONCPP_WITH_TESTS OFF)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF)
FetchContent_MakeAvailable(jsoncpp)

FetchContent_Declare(httplib SYSTEM
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib
    GIT_TAG e64379c3d71ccf3f62e4e4853bfd1316901564b3 # v0.18.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(httplib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(simplestream)

add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)
add_compile_options(-Wall -Wextra -Wno-dangling-reference)
# Supress false positive dangling reference warnings in GCC 13 
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-dangling-reference) 
endif()

add_executable(simplestream main.cpp)

target_link_libraries(simplestream jsoncpp httplib)
