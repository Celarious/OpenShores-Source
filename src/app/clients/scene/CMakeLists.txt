au_add_executable(AuSceneClient "${CMAKE_CURRENT_SOURCE_DIR}")
if(APPLE)
    set_target_properties(AuSceneClient PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "OpenShores"
        MACOSX_BUNDLE_GUI_IDENTIFIER "net.openshores.client"
    )
endif()
add_dependencies(AuSceneClient AuClientRuntimeData)
target_link_libraries(AuSceneClient PRIVATE
    AuClientRes AuDesigner AuClient AuAtom AuData AuLoginClient AuMail
    AuGlobal AuUtil AuMath AuGL AuAtomRes AuModelRes AuBodyPartsRes
    AuTexture AuClientMailRes AuStoryRes
)
