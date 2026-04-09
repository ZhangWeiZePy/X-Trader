include_directories(${CMAKE_INCLUDE_PATH})
link_directories(${CMAKE_LIBRARY_PATH})

add_executable(tora_market_test tora_market_test.cpp)
target_link_libraries(tora_market_test framework)

add_executable(tora_trader_test tora_trader_test.cpp)
target_link_libraries(tora_trader_test framework)