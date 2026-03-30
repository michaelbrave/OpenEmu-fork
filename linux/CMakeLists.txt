cmake_minimum_required(VERSION 3.18)
project(OpenEMU_Linux C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets Sql Network)
find_package(SDL2 REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(oe_core_bridge STATIC
    src/oe_core_bridge.cpp
    src/oe_library_database.cpp
    src/oe_audio_backend.cpp
    src/oe_input_backend.cpp
)
target_include_directories(oe_core_bridge PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_libraries(oe_core_bridge PUBLIC Qt6::Sql Qt6::OpenGLWidgets Qt6::OpenGL ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS} GL)

add_subdirectory(cores/mgba)
add_subdirectory(cores/snes9x)

set(OE_BUILD_FCEU OFF)
set(OE_ZLIB_INCLUDE_DIR "")
if (EXISTS "/usr/include/zlib.h")
    set(OE_BUILD_FCEU ON)
    set(OE_ZLIB_INCLUDE_DIR "/usr/include")
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/zlib.h")
    set(OE_BUILD_FCEU ON)
    set(OE_ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1")

    add_library(oe_zlib STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/adler32.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/compress.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/crc32.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/deflate.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/gzclose.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/gzlib.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/gzread.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/gzwrite.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/inflate.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/infback.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/inftrees.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/inffast.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/trees.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/uncompr.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/zutil.c
    )
    target_include_directories(oe_zlib PUBLIC "${OE_ZLIB_INCLUDE_DIR}")
    set_target_properties(oe_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    message(WARNING "Skipping FCEU: zlib headers not found (system or vendored)")
endif()

if (OE_BUILD_FCEU)
    add_subdirectory(cores/fceu)
endif()

add_subdirectory(test)

add_executable(qt_test test/qt_test.cpp)
target_link_libraries(qt_test PRIVATE Qt6::Widgets oe_core_bridge)

add_executable(db_test test/db_test.cpp)
target_link_libraries(db_test PRIVATE Qt6::Sql oe_core_bridge)

add_executable(audio_input_test test/audio_input_test.cpp)
target_link_libraries(audio_input_test PRIVATE Qt6::Core oe_core_bridge)

qt_add_executable(openemu-linux
    src/main.cpp
    src/oe_emulator_view.cpp
    src/oe_game_loop.cpp
    src/oe_library_ui.cpp
    src/oe_cover_art.cpp
    src/oe_games_api.cpp
    src/oe_preferences_dialog.cpp
    src/oe_edit_game_info_dialog.cpp
)
target_link_libraries(openemu-linux PRIVATE oe_core_bridge Qt6::Widgets Qt6::OpenGLWidgets Qt6::Network)
