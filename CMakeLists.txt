cmake_minimum_required(VERSION 3.22...3.28)

project(NOVA LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
else()
    message(STATUS "Feel free to sudo apt install ccache for likely improved compile / build times.")
endif()

add_executable(main src/main.cpp)

# Include new headers
target_include_directories(main PUBLIC "${CMAKE_SOURCE_DIR}/include")

# dv-processing APIs
find_package(dv-processing CONFIG REQUIRED)
target_link_libraries(main
    PRIVATE
    dv::processing
)

if (MSVC)
    target_compile_options(main PRIVATE /W4 /WX)
else()
    target_compile_options(main PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror -Wno-error=unused-parameter -Wno-error=unused-but-set-variable>)
endif()

