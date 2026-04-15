include_directories(${CMAKE_INCLUDE_PATH})
link_directories(${CMAKE_LIBRARY_PATH})

set(BINARY_NAME femtotrader)
aux_source_directory(. SRCs)

add_executable(${BINARY_NAME} ${SRCs})
target_link_libraries(${BINARY_NAME} framework ${SPDLOG_TARGET})