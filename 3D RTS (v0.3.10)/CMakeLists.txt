cmake_minimum_required(VERSION 3.16)
project(RTSCameraTerrain LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RTS_BUILD_DEMO "Build OpenGL demo application" ON)
include(CTest)

add_library(RTSNavigation STATIC NavigationGrid.cpp)
target_include_directories(RTSNavigation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(RTSAuthoring STATIC TerrainMap.cpp DataCatalog.cpp)
target_include_directories(RTSAuthoring PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(RTSSimulation STATIC
    RTSSkirmish.cpp
    unit_navigationagent3d_implementation.cpp
    unitdata_resource_implementation.cpp)
target_include_directories(RTSSimulation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(RTSSimulation PUBLIC RTSNavigation)

if(BUILD_TESTING)
    add_executable(RTSNavigationTests tests/navigation_tests.cpp)
    target_link_libraries(RTSNavigationTests PRIVATE RTSNavigation)
    add_test(NAME navigation COMMAND RTSNavigationTests)

    add_executable(RTSSimulationTests tests/skirmish_tests.cpp)
    target_link_libraries(RTSSimulationTests PRIVATE RTSSimulation)
    add_test(NAME skirmish COMMAND RTSSimulationTests)

    add_executable(RTSUnitDataTests tests/unitdata_tests.cpp)
    target_link_libraries(RTSUnitDataTests PRIVATE RTSSimulation)
    add_test(NAME unitdata COMMAND RTSUnitDataTests)

    add_executable(RTSChaseTests tests/chase_tests.cpp)
    target_link_libraries(RTSChaseTests PRIVATE RTSSimulation)
    add_test(NAME chase COMMAND RTSChaseTests)

    add_executable(RTSTerrainTests tests/terrain_tests.cpp)
    target_link_libraries(RTSTerrainTests PRIVATE RTSAuthoring)
    add_test(NAME terrain COMMAND RTSTerrainTests)

    add_executable(RTSCatalogTests tests/catalog_tests.cpp)
    target_link_libraries(RTSCatalogTests PRIVATE RTSAuthoring)
    add_test(NAME catalog COMMAND RTSCatalogTests)
endif()

if(RTS_BUILD_DEMO)
    find_package(glfw3 QUIET)
    if(NOT TARGET glfw)
        include(FetchContent)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4)
        FetchContent_MakeAvailable(glfw)
    endif()
    find_package(OpenGL REQUIRED)
    add_executable(RTSCameraTerrain
        main_entry_point_render_loop.cpp
        opengl_3_3_core_function_loader_implementation.cpp
        rts_camera_rig_implementation.cpp
        selection_manager_implementation.cpp)
    target_link_libraries(RTSCameraTerrain PRIVATE glfw OpenGL::GL RTSSimulation)
endif()

if(MSVC)
    target_compile_options(RTSNavigation PRIVATE /W4)
    target_compile_options(RTSSimulation PRIVATE /W4)
    target_compile_options(RTSAuthoring PRIVATE /W4)
else()
    target_compile_options(RTSNavigation PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(RTSSimulation PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(RTSAuthoring PRIVATE -Wall -Wextra -Wpedantic)
endif()
