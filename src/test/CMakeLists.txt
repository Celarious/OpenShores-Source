add_executable(RunTests)
target_sources(RunTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp)
target_link_libraries(RunTests PRIVATE doctest AuUtil)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/**/*.h"
)

target_sources(RunTests PRIVATE ${sources})
