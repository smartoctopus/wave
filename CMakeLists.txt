cmake_minimum_required(VERSION 3.15) # 3.15 needs to be used for generator expressions

project(wave)

if(MSVC)
  add_compile_options(/nologo /W3 /WX
    $<$<CONFIG:DEBUG>/Od>
    $<$<CONFIG:DEBUG>/FC>
    $<$<CONFIG:DEBUG>/Zo>
    $<$<CONFIG:DEBUG>/Zi>
    $<$<CONFIG:RELEASE>/O2>
  )
else()
  add_compile_options(-std=c99 -W -Wall -Wno-implicit-fallthrough -Werror -pedantic
    $<$<CONFIG:DEBUG>:-O0>
    $<$<CONFIG:DEBUG>:-g3>
    $<$<CONFIG:DEBUG>:-ggdb>
    $<$<CONFIG:RELEASE>:-O3>
    $<$<CONFIG:RELEASE>:-DNDEBUG>
  )
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    src/compiler/main.c
    src/compiler/lexer.c
    src/compiler/ast.c
    src/compiler/vfs.c
    src/compiler/diagnostic.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories( ${PROJECT_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/src/compiler/include
)

target_include_directories( ${PROJECT_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/testing
)
