
cmake_minimum_required(VERSION 3.16)
project(RTSCameraTerrain LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

find_package(OpenGL REQUIRED)

add_executable(RTSCameraTerrain
    main_entry_point_render_loop.cpp
    opengl_3_3_core_function_loader_implementation.cpp
    rts_camera_rig_implementation.cpp
    selection_manager_implementation.cpp
    unitdata_resource_implementation.cpp
    unit_navigationagent3d_implementation.cpp

)

target_link_libraries(RTSCameraTerrain PRIVATE
    glfw
    OpenGL::GL
)
