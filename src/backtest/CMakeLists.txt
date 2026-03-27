include_directories(${CMAKE_INCLUDE_PATH})

set(BACKTEST_LIB xtrader_backtest)
aux_source_directory(. BACKTEST_SRCS)

add_library(${BACKTEST_LIB} STATIC ${BACKTEST_SRCS})
target_include_directories(${BACKTEST_LIB} PUBLIC ${PROJECT_SOURCE_DIR}/backtest)
