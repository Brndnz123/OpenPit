cmake_minimum_required(VERSION 3.16)
project(OpenPit3D VERSION 4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W3 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wno-unused-variable -Wno-unused-parameter)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(glew-cmake_BUILD_STATIC ON  CACHE BOOL "" FORCE)
set(ONLY_LIBS               ON  CACHE BOOL "" FORCE)
FetchContent_Declare(glew
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG        glew-cmake-2.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glew)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)
if(MSVC)
    target_compile_options(imgui_lib PRIVATE /W0)
else()
    target_compile_options(imgui_lib PRIVATE -w)
endif()

find_package(OpenMP)

add_executable(OpenPit3D
    src/main.cpp
    src/BlockModel.cpp
    src/Camera.cpp
    src/DrillholeDatabase.cpp
    src/LGOptimizer.cpp
    src/MeshModel.cpp
    src/PitGenerator.cpp
    src/Renderer.cpp
    src/SceneController.cpp
    src/Shader.cpp
    src/UI.cpp
    src/CashFlowAnalysis.cpp
)

target_include_directories(OpenPit3D PRIVATE src)

target_link_libraries(OpenPit3D PRIVATE
    glfw
    libglew_static
    glm
    imgui_lib
    $<$<BOOL:${OpenMP_CXX_FOUND}>:OpenMP::OpenMP_CXX>
)

if(WIN32)
    target_link_libraries(OpenPit3D PRIVATE opengl32)
elseif(APPLE)
    find_library(OPENGL_LIB OpenGL REQUIRED)
    target_link_libraries(OpenPit3D PRIVATE ${OPENGL_LIB})
elseif(UNIX)
    target_link_libraries(OpenPit3D PRIVATE GL)
endif()

message(STATUS "")
message(STATUS "=== OpenPit3D Build ===")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenMP: ${OpenMP_CXX_FOUND}")
message(STATUS "========================")
message(STATUS "")
