if(WIN32)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH}
    "${PROJECT_SOURCE_DIR}/../api/ctp/v6.7.2/win64/"
    "${PROJECT_SOURCE_DIR}/../api/xtp/v6.6.9/win64/")
else()
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH}
    "${PROJECT_SOURCE_DIR}/../api/ctp/v6.7.2/linux64/"
    "${PROJECT_SOURCE_DIR}/../api/xtp/v6.6.9/linux64/")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/utils)
include_directories(${CMAKE_INCLUDE_PATH})

aux_source_directory(market MARKET)
aux_source_directory(trader TRADER)

add_library(trade-core STATIC interface.cpp ${MARKET} ${TRADER})

if(UNIX)
    target_link_libraries(trade-core dl)
endif()
