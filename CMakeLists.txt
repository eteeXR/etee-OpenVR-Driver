cmake_minimum_required(VERSION "3.7.1")

# Solution
project("etee_driver")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Deps
set(OPENVR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libraries/openvr/headers")
set(MINHOOK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libraries/minhook/include")
set(TINYGLTF_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libraries/tinygltf")

set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PROCESSOR_ARCH "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(PROCESSOR_ARCH "32")
endif()

if(WIN32)
    set(PLATFORM_NAME "win")
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_NAME "linux")
elseif(APPLE)
    set(PLATFORM_NAME "osx")
endif()

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get the latest commit hash
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

find_library(OPENVR_LIB openvr_api HINTS "${CMAKE_CURRENT_SOURCE_DIR}/libraries/openvr/lib/${PLATFORM_NAME}${PROCESSOR_ARCH}/" NO_DEFAULT_PATH )
find_library(MINHOOK_LIB minhook HINTS "${CMAKE_CURRENT_SOURCE_DIR}/libraries/minhook/lib/" NO_DEFAULT_PATH )

set(DRIVER_NAME "etee")
set(ETEE_PROJECT "driver_${DRIVER_NAME}")

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library("${ETEE_PROJECT}" SHARED "${HEADERS}" "${SOURCES}")

target_include_directories("${ETEE_PROJECT}" PUBLIC "${OPENVR_INCLUDE_DIR}" "${TINYGLTF_INCLUDE_DIR}" "${MINHOOK_INCLUDE_DIR}")

target_include_directories("${ETEE_PROJECT}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_link_libraries("${ETEE_PROJECT}" PUBLIC "${OPENVR_LIB}" "${MINHOOK_LIB}" wsock32.lib ws2_32.lib setupapi)

target_compile_definitions("${ETEE_PROJECT}" PRIVATE
    "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES ${HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES ${SOURCES})
set_property(TARGET "${ETEE_PROJECT}" PROPERTY CXX_STANDARD 17)

# Copy driver assets to output folder
add_custom_command(
        TARGET ${ETEE_PROJECT}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/${DRIVER_NAME}
        $<TARGET_FILE_DIR:${ETEE_PROJECT}>/${DRIVER_NAME}
)

# Copy dll to output folder
add_custom_command(
        TARGET ${ETEE_PROJECT}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${ETEE_PROJECT}>
        $<TARGET_FILE_DIR:${ETEE_PROJECT}>/${DRIVER_NAME}/bin/${PLATFORM_NAME}${PROCESSOR_ARCH}/$<TARGET_FILE_NAME:${ETEE_PROJECT}>
)