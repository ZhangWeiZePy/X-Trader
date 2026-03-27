include_directories(${CMAKE_INCLUDE_PATH})
link_directories(${CMAKE_LIBRARY_PATH})

add_executable(limitup_replay_test limitup_replay_test.cpp)
target_include_directories(limitup_replay_test PRIVATE ${PROJECT_SOURCE_DIR}/backtest ${PROJECT_SOURCE_DIR}/strategy)
target_link_libraries(limitup_replay_test xtrader_backtest framework)
