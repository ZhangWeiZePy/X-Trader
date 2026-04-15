if (NOT DEFINED CTP_API_DIR)
	if (WIN32)
		set(CTP_API_DIR "${PROJECT_SOURCE_DIR}/api/ctp/v6.7.2/win64")
	else ()
		set(CTP_API_DIR "${PROJECT_SOURCE_DIR}/api/ctp/v6.7.2/linux64")
	endif ()
endif ()

if (NOT DEFINED XTP_API_DIR)
	if (WIN32)
		set(XTP_API_DIR "${PROJECT_SOURCE_DIR}/api/xtp/v6.6.9/win64")
	else ()
		set(XTP_API_DIR "${PROJECT_SOURCE_DIR}/api/xtp/v6.6.9/linux64")
	endif ()
endif ()

if (NOT DEFINED TORA_API_DIR)
	if (WIN32)
		set(TORA_API_DIR "${PROJECT_SOURCE_DIR}/api/tora/v4.0.8/win64")
	else ()
		set(TORA_API_DIR "${PROJECT_SOURCE_DIR}/api/tora/v4.0.8/linux64")
	endif ()
endif ()

aux_source_directory(market MARKET)
aux_source_directory(trader TRADER)

add_library(trade-core STATIC interface.cpp ${MARKET} ${TRADER})

target_include_directories(trade-core
	PRIVATE
	"${PROJECT_SOURCE_DIR}/src/include"
	"${PROJECT_SOURCE_DIR}/src/utils"
	"${CTP_API_DIR}"
	"${XTP_API_DIR}"
	"${TORA_API_DIR}"
)

if (UNIX)
	target_link_libraries(trade-core dl ${SPDLOG_TARGET})
else ()
	target_link_libraries(trade-core ${SPDLOG_TARGET})
endif ()
