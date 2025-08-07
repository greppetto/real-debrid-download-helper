cmake_minimum_required(VERSION 3.31)
project(alpha VERSION 0.1.0 LANGUAGES CXX)

# Prevent src loc builds
if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate 'build' directory and build in there.")
endif()

include(FetchContent)

# Fetch CPR
FetchContent_Declare(
  cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG        da40186618909b1a7363d4e4495aa899c6e0eb75
)
set(CPR_USE_SYSTEM_CURL ON)
FetchContent_MakeAvailable(cpr)

# Fetch JSON
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wconversion -pedantic -Wfatal-errors)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W3 /WX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

set(SOURCES src/main.cpp)
set(EXECUTABLE_NAME rd2a)
set(LIBRARY_SOURCES src/tui.cpp src/util.cpp)
set(LIBRARIES_NAME helperlibs)

add_library(${LIBRARIES_NAME} ${LIBRARY_SOURCES})
add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_link_libraries(${EXECUTABLE_NAME} PRIVATE cpr::cpr nlohmann_json::nlohmann_json)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${LIBRARIES_NAME})
