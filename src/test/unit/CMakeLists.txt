include_directories(${CMAKE_INCLUDE_PATH})

set(RISK_SRCS
    ${PROJECT_SOURCE_DIR}/strategy/risk/position_sizer.cpp
    ${PROJECT_SOURCE_DIR}/strategy/risk/cancel_rate_guard.cpp
    ${PROJECT_SOURCE_DIR}/strategy/risk/risk_guard.cpp
)

add_executable(risk_unit_test risk_unit_test.cpp ${RISK_SRCS})
target_include_directories(risk_unit_test PRIVATE ${PROJECT_SOURCE_DIR}/strategy)
