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
    set(XTP_API_DIR "${PROJECT_SOURCE_DIR}/../api/xtp/v6.6.9/win64")
else()
    set(CTP_API_DIR "${PROJECT_SOURCE_DIR}/../api/ctp/v6.7.2/linux64")
    set(XTP_API_DIR "${PROJECT_SOURCE_DIR}/../api/xtp/v6.6.9/linux64")
endif()

# Create lib/ctp directory in bin
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lib/ctp)
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lib/xtp)

# Copy dynamic libraries to bin/lib/ctp and bin/lib/xtp
if(WIN32)
    file(GLOB CTP_LIBS "${CTP_API_DIR}/*.dll")
    file(GLOB XTP_LIBS "${XTP_API_DIR}/*.dll")
else()
    file(GLOB CTP_LIBS "${CTP_API_DIR}/*.so")
    file(GLOB XTP_LIBS "${XTP_API_DIR}/*.so")
endif()
file(COPY ${CTP_LIBS} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/lib/ctp)
file(COPY ${XTP_LIBS} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/lib/xtp)

add_subdirectory(trade-core)
add_subdirectory(framework)
add_subdirectory(strategy)
add_subdirectory(backtest)
add_subdirectory(test/xtp)
add_subdirectory(test/unit)
add_subdirectory(test/integration)
