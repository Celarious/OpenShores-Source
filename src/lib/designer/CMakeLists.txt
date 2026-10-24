au_add_static_library(AuDesigner "${CMAKE_CURRENT_SOURCE_DIR}" DEFINE AUDESIGNER)
target_link_libraries(AuDesigner PUBLIC
    AuUtil AuMath AuGlobal AuGL AuData AuAtom AuLoginClient AuMail AuClient AuOldShip
    SDL2::SDL2
)
target_include_directories(AuDesigner PRIVATE
    "${CMAKE_SOURCE_DIR}/src/app/servers/scene"
)
