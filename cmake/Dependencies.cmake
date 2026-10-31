find_package(Qt5 5.12 REQUIRED COMPONENTS
    Core
    Gui
    Network
    OpenGL
    Sql
    Widgets
)

find_package(OpenGL REQUIRED)
find_package(OpenAL REQUIRED)
if(NOT TARGET OpenAL::OpenAL)
    add_library(OpenAL::OpenAL UNKNOWN IMPORTED)
    set_target_properties(OpenAL::OpenAL PROPERTIES
        IMPORTED_LOCATION "${OPENAL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENAL_INCLUDE_DIR}"
    )
endif()

find_package(SDL2 CONFIG QUIET)
if(NOT TARGET SDL2::SDL2)
    find_path(SDL2_INCLUDE_DIR
        NAMES SDL.h
        HINTS ENV SDL2_ROOT
        PATH_SUFFIXES include include/SDL2 SDL2
    )
    find_library(SDL2_LIBRARY
        NAMES SDL2
        HINTS ENV SDL2_ROOT
        PATH_SUFFIXES lib lib64 lib/x64
    )

    if(NOT SDL2_INCLUDE_DIR OR NOT SDL2_LIBRARY)
        message(FATAL_ERROR
            "SDL2 was not found. Install its development package or set SDL2_ROOT."
        )
    endif()

    add_library(SDL2::SDL2 UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
    )
endif()

if(NOT TARGET OpenGL::GLU)
    if(WIN32)
        add_library(OpenGL::GLU INTERFACE IMPORTED)
        set_property(TARGET OpenGL::GLU PROPERTY
            INTERFACE_LINK_LIBRARIES glu32
        )
    elseif(APPLE)
        # GLU is part of Apple's OpenGL framework, rather than a separate dylib.
        add_library(OpenGL::GLU INTERFACE IMPORTED)
        set_property(TARGET OpenGL::GLU PROPERTY
            INTERFACE_LINK_LIBRARIES OpenGL::GL
        )
    else()
        find_library(GLU_LIBRARY NAMES GLU REQUIRED)
        add_library(OpenGL::GLU UNKNOWN IMPORTED)
        set_target_properties(OpenGL::GLU PROPERTIES
            IMPORTED_LOCATION "${GLU_LIBRARY}"
        )
    endif()
endif()
