au_add_static_library(AuOldShip "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(AuOldShip PUBLIC AuOldShipGen AuData AuGlobal AuUtil AuMath AuGL)
