include_directories(${CMAKE_INCLUDE_PATH})
link_directories(${CMAKE_LIBRARY_PATH})

add_executable(xtp_market_test xtp_market_test.cpp)
target_link_libraries(xtp_market_test framework)

add_executable(xtp_trader_test xtp_trader_test.cpp)
target_link_libraries(xtp_trader_test framework)
