cmake_minimum_required(VERSION 3.18)

project(x-trader)

if (UNIX)
	add_compile_options(-fPIC)
endif ()

set(CMAKE_CXX_STANDARD 20)


set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

set(CMAKE_CXX_FLAGS_RELEASE"-O2")


set(
	CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH}
	"${PROJECT_SOURCE_DIR}/src/include"
	"${PROJECT_SOURCE_DIR}/src/utils"
)


set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH}
	"bin")
if (WIN32)
	set(CTP_API_DIR "${PROJECT_SOURCE_DIR}/api/ctp/v6.7.2/win64")
	set(XTP_API_DIR "${PROJECT_SOURCE_DIR}/api/xtp/v6.6.9/win64")
	set(TORA_API_DIR "${PROJECT_SOURCE_DIR}/api/tora/v4.0.8/win64")
else ()
	set(CTP_API_DIR "${PROJECT_SOURCE_DIR}/api/ctp/v6.7.2/linux64")
	set(XTP_API_DIR "${PROJECT_SOURCE_DIR}/api/xtp/v6.6.9/linux64")
	set(TORA_API_DIR "${PROJECT_SOURCE_DIR}/api/tora/v4.0.8/linux64")
endif ()

# Create lib/ctp directory in bin
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lib/ctp)
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lib/xtp)
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lib/tora)

# Copy dynamic libraries to bin/lib/ctp and bin/lib/xtp
if (WIN32)
	file(GLOB XTP_LIBS "${XTP_API_DIR}/*.dll")
	file(GLOB TORA_LIBS "${TORA_API_DIR}/*.dll")
else ()
	file(GLOB XTP_LIBS "${XTP_API_DIR}/*.so")
	file(GLOB TORA_LIBS "${TORA_API_DIR}/*.so")
endif ()
file(COPY ${XTP_LIBS} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/lib/xtp)
file(COPY ${TORA_LIBS} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/lib/tora)

add_subdirectory(src/trade-core)
add_subdirectory(src/framework)
add_subdirectory(src/strategy)
add_subdirectory(src/test/xtp)
add_subdirectory(src/test/tora)
add_subdirectory(src/test/config)
