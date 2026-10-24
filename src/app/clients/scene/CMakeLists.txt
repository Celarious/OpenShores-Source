au_add_executable(AuSceneClient "${CMAKE_CURRENT_SOURCE_DIR}")
add_dependencies(AuSceneClient AuClientRuntimeData)
target_link_libraries(AuSceneClient PRIVATE
    AuClientRes AuDesigner AuOldShip AuClient AuAtom AuData AuLoginClient AuMail
    AuGlobal AuUtil AuMath AuGL AuAtomRes AuModelRes AuBodyPartsRes
    AuTexture AuClientMailRes AuStoryRes
)
