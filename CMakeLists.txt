cmake_minimum_required(VERSION 3.25)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
FetchContent_Declare(
    SDL2
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
    GIT_TAG        f461d91cd265d7b9a44b4d472b1df0c0ad2855a0 #release-2.30.2
    GIT_PROGRESS   TRUE
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL2)

project(legacy)

file(GLOB imgui_src CONFIGURE_DEPENDS imgui/* imgui/backends/*)

add_executable(${PROJECT_NAME} WIN32
    ${imgui_src}

    src/rule.hpp
    src/tile.hpp
    src/rule_algo.hpp
    src/dear_imgui.hpp
    src/common.hpp

    src/apply_rule.cpp
    src/edit_rule.cpp
    src/docs.cpp
    src/load_rule.cpp
    src/frame_main.cpp
    src/main.cpp
)

# https://stackoverflow.com/questions/47690822/possible-to-force-cmake-msvc-to-use-utf-8-encoding-for-source-files-without-a-bo
target_compile_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_include_directories(${PROJECT_NAME} PRIVATE src imgui imgui/backends)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)

# https://stackoverflow.com/questions/10671916/how-to-copy-dll-files-into-the-same-folder-as-the-executable-using-cmake
# (Taking Gnimuc's answer)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS
)