cmake_minimum_required(VERSION 3.22.0)
project("SDL3_GPU_examples" VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)

# fmt
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 11.0.2
    CMAKE_ARGS -DFMT_DOC=OFF -DFMT_INSTALL=OFF -DFMT_TEST=OFF
    UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(fmt)

if(IS_DIRECTORY "${fmt_SOURCE_DIR}")
    set_property(DIRECTORY ${fmt_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)
endif()

# SPIRV-Headers
FetchContent_Declare(
    spirv-headers
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
    GIT_TAG main
    UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(spirv-headers)

if(IS_DIRECTORY "${spirv-headers_SOURCE_DIR}")
    set_property(DIRECTORY ${spirv-headers_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)
endif()

# SPIRV-Tools
FetchContent_Declare(
    spirv-tools
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
    GIT_TAG main
    CMAKE_ARGS -DSPIRV_SKIP_EXECUTABLES=ON -DSPIRV_SKIP_TESTS=ON
    UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(spirv-tools)

if(IS_DIRECTORY "${spirv-tools_SOURCE_DIR}")
    set_property(DIRECTORY ${spirv-tools_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)
endif()

# glslang
FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG main
    UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(glslang)

if(IS_DIRECTORY "${glslang_SOURCE_DIR}")
    set_property(DIRECTORY ${glslang_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)
endif()

# SDL3
FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main
    UPDATE_DISCONNECTED ON
)

FetchContent_MakeAvailable(SDL)

if(IS_DIRECTORY "${SDL_SOURCE_DIR}")
    set_property(DIRECTORY ${SDL_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)
endif()

# Application
set(APP_NAME "triangle")
add_executable(${APP_NAME}
    source/triangle.cpp
)

target_compile_options(${APP_NAME} PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
         -pedantic -Wall -Wextra -Werror>
     $<$<CXX_COMPILER_ID:MSVC>:
          /W3 /WX>
)

target_compile_features(${APP_NAME}
    PUBLIC
        cxx_std_23
)

target_compile_definitions(${APP_NAME} 
    PUBLIC
        SDL_MAIN_USE_CALLBACKS=1
)

target_link_libraries(${APP_NAME}
    PUBLIC
        SDL3::SDL3
        fmt::fmt
        glslang
        glslang-default-resource-limits
        SPIRV
)
