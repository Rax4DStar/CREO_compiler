cmake_minimum_required(VERSION 3.26)

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

    add_executable(semantic_tests
        tests/SemanticAnalyserTests.cpp
    )

    add_executable(ir_generator_tests
        tests/IRGeneratorTests.cpp
    )

    target_include_directories(parser_tests PRIVATE include)
    target_link_libraries(semantic_tests PRIVATE creo_frontend)
    target_link_libraries(ir_generator_tests PRIVATE creo_frontend)

    add_test(
        NAME parser_tests
        COMMAND parser_tests
    )

    add_test(
        NAME semantic_tests
        COMMAND semantic_tests
    )

    add_test(
        NAME ir_generator_tests
        COMMAND ir_generator_tests
    )

    add_test(
        NAME semantic_sample
        COMMAND creo_compiler
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-2.creo
    )

    add_test(
        NAME compiler_ir_smoke_test
        COMMAND creo_compiler
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-3.creo
    )
endif()
