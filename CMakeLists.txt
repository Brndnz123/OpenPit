cmake_minimum_required(VERSION 3.24)
project(OpenPit3D VERSION 1.0 LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. System Dependencies: OpenMP and OpenGL
find_package(OpenMP REQUIRED)
find_package(OpenGL REQUIRED)

include(FetchContent)

# 2. GLFW
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 3. GLEW (Using the corrected CMake-friendly mirror so it doesn't prompt for password)
FetchContent_Declare(glew
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG        glew-cmake-2.2.0
)
FetchContent_MakeAvailable(glew)

# 4. GLM (Math library)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

# 5. Eigen (Matrix math for kriging)
FetchContent_Declare(eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG        3.4.0
)
FetchContent_MakeAvailable(eigen)

# 6. ImGui
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.89.9
)
FetchContent_Populate(imgui)

# Create ImGui as a static library
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
target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)


# 7. OpenPit3D Executable (all your 13 source files)
set(SOURCES
    main.cpp
    BlockModel.cpp
    Camera.cpp
    CashFlowAnalysis.cpp
    LGOptimizer.cpp
    MeshModel.cpp
    PitGenerator.cpp
    PitScheduler.cpp
    Renderer.cpp
    SceneController.cpp
    Shader.cpp
    TextDrillholeData.cpp
    UI.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# 8. Link everything together
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw
    libglew_static
    imgui_lib
    OpenMP::OpenMP_CXX
)

# Add GLM and Eigen to the include path
target_include_directories(${PROJECT_NAME} PRIVATE
    ${glm_SOURCE_DIR}
    ${eigen_SOURCE_DIR}
)

# Apple specific OS frameworks if building on macOS in the future
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        "-framework OpenGL" 
        "-framework Cocoa" 
        "-framework IOKit" 
        "-framework CoreVideo"
    )
endif()
