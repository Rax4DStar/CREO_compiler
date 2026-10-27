cmake_minimum_required(VERSION 3.25)

project(creo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(creo_frontend ${SOURCES})

target_include_directories(creo_frontend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(creo_compiler src/main.cpp)

target_link_libraries(creo_compiler
    PRIVATE
        creo_frontend
)

option(BUILD_TESTING "Build the creo tests" ON)

if(BUILD_TESTING)
    enable_testing()

    add_executable(parser_tests
        tests/ParserTests.cpp
        ${SOURCES}
    )

    target_include_directories(parser_tests PRIVATE include)

    add_test(
        NAME parser_tests
        COMMAND parser_tests
    )

    add_test(
        NAME semantic_sample
        COMMAND creo_compiler
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.creo
    )
endif()
