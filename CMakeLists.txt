cmake_minimum_required(VERSION 3.22...3.28)

project(NOVA LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
else()
    message(STATUS "If you want, install 'ccache' for faster (on average) compile / build times.")
endif()

IF(${SOL})
	FILE(GLOB_RECURSE SOURCES "src0/*.cpp")
	FILE(GLOB_RECURSE HEADERS "include0/*.h")
ELSE()
	FILE(GLOB_RECURSE SOURCES "src/*.cpp")
	FILE(GLOB_RECURSE HEADERS "include/*.h")
ENDIF()
FILE(GLOB_RECURSE GLSL "resources/*.glsl")

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/include/imgui")

file(GLOB IMGUI_CORE_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
)

file(GLOB IMGUI_BACKENDS
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
)

list(APPEND SOURCES ${IMGUI_CORE_SOURCES} ${IMGUI_BACKENDS})

# print cpp sources
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${GLSL})

# Include new headers
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/imgui"
    "${CMAKE_SOURCE_DIR}/include/imgui/backends"
)

find_package(GLEW CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(dv-processing CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GLEW::GLEW
    glfw
    glm::glm
    Eigen3::Eigen
    dv::processing
)

# OS specific options and libraries
IF(WIN32)
	# -Wall produces way too many warnings.
	# -pedantic is not supported.
	# Disable warning 4996.
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE opengl32)
	SET_PROPERTY(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME})
ENDIF()

# We are only on MSVC for now.
if (MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror -Wno-error=unused-parameter -Wno-error=unused-but-set-variable>)
endif()

