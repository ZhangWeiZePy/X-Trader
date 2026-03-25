cmake_minimum_required(VERSION 3.18)

project(x-trader)

if(UNIX)
    add_compile_options(-fPIC)
endif()

set(CMAKE_CXX_STANDARD 20)


set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin)
#set(CMAKE_DEBUG_POSTFIX "_d")
set(CMAKE_CXX_FLAGS_RELEASE"-O2")


set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH}
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/utils")


set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH}
    "${PROJECT_SOURCE_DIR}/../bin")
if(WIN32)
    set(CTP_API_DIR "${PROJECT_SOURCE_DIR}/../api/ctp/v6.7.2/win64")
else()
    set(CTP_API_DIR "${PROJECT_SOURCE_DIR}/../api/ctp/v6.7.2/linux64")
endif()

# Create lib/ctp directory in bin
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lib/ctp)

# Copy dynamic libraries to bin/lib/ctp
if(WIN32)
    file(GLOB CTP_LIBS "${CTP_API_DIR}/*.dll")
else()
    file(GLOB CTP_LIBS "${CTP_API_DIR}/*.so")
endif()
file(COPY ${CTP_LIBS} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/lib/ctp)

add_subdirectory(trade-core)
add_subdirectory(framework)
add_subdirectory (strategy)
