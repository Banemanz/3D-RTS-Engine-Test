cmake_minimum_required(VERSION 3.16)
project(RTSCameraTerrain LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RTS_BUILD_DEMO "Build OpenGL demo application" ON)
# Every platform compiles the same feature configuration; only platform
# dependencies differ. The git revision is embedded in the native executable.
execute_process(COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE RTS_SOURCE_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT RTS_SOURCE_COMMIT)
    set(RTS_SOURCE_COMMIT "unknown")
endif()
include(CTest)

add_library(RTSNavigation STATIC NavigationGrid.cpp)
target_include_directories(RTSNavigation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(RTSAuthoring STATIC TerrainMap.cpp DataCatalog.cpp RTSProject.cpp EditorSession.cpp SceneEditor.cpp InstanceOverrides.cpp TriggerDefinition.cpp)
target_include_directories(RTSAuthoring PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(RTSSimulation STATIC
    RTSSkirmish.cpp
    RTSVision.cpp
    RTSFormation.cpp
    UnitSpatialIndex.cpp
    TriggerEngine.cpp
    unit_navigationagent3d_implementation.cpp
    unitdata_resource_implementation.cpp)
target_include_directories(RTSSimulation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(RTSSimulation PUBLIC RTSNavigation RTSAuthoring)

if(BUILD_TESTING)
    add_executable(RTSNativeMenuTests tests/native_menu_tests.cpp)
    target_include_directories(RTSNativeMenuTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME native_menu COMMAND RTSNativeMenuTests)
    add_executable(RTSDynamicNavigationTests tests/dynamic_navigation_tests.cpp)
    target_link_libraries(RTSDynamicNavigationTests PRIVATE RTSSimulation)
    add_test(NAME dynamic_navigation COMMAND RTSDynamicNavigationTests)

    add_executable(RTSNavigationTests tests/navigation_tests.cpp)
    target_link_libraries(RTSNavigationTests PRIVATE RTSNavigation)
    add_test(NAME navigation COMMAND RTSNavigationTests)

    add_executable(RTSFormationTests tests/formation_tests.cpp)
    target_link_libraries(RTSFormationTests PRIVATE RTSSimulation)
    add_test(NAME formation COMMAND RTSFormationTests)

    add_executable(RTSOrdersTests tests/order_tests.cpp)
    add_executable(RTSCombatResolutionTests tests/combat_resolution_tests.cpp)
    target_link_libraries(RTSCombatResolutionTests PRIVATE RTSSimulation)
    add_test(NAME combat_resolution COMMAND RTSCombatResolutionTests)
    target_link_libraries(RTSOrdersTests PRIVATE RTSSimulation)
    add_test(NAME orders COMMAND RTSOrdersTests)

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
    add_executable(RTSCatalogRenameTests tests/catalog_rename_tests.cpp)
    target_link_libraries(RTSCatalogRenameTests PRIVATE RTSSimulation)
    add_test(NAME catalog_rename COMMAND RTSCatalogRenameTests)
    add_executable(RTSProductionSpawnTests tests/production_spawn_tests.cpp)
    target_link_libraries(RTSProductionSpawnTests PRIVATE RTSSimulation)
    add_test(NAME production_spawn COMMAND RTSProductionSpawnTests)
    add_executable(RTSProductionCatalogTests tests/production_catalog_tests.cpp)
    target_link_libraries(RTSProductionCatalogTests PRIVATE RTSSimulation)
    add_test(NAME production_catalog COMMAND RTSProductionCatalogTests)
    add_executable(RTSCatalogDeletionTests tests/catalog_deletion_tests.cpp)
    target_link_libraries(RTSCatalogDeletionTests PRIVATE RTSSimulation)
    add_test(NAME catalog_deletion COMMAND RTSCatalogDeletionTests)
    add_executable(RTSProjectTests tests/project_tests.cpp)
    target_link_libraries(RTSProjectTests PRIVATE RTSAuthoring)
    add_test(NAME project COMMAND RTSProjectTests)

    add_executable(RTSReferenceAuthoringTests tests/reference_authoring_tests.cpp)
    target_link_libraries(RTSReferenceAuthoringTests PRIVATE RTSAuthoring)
    add_test(NAME reference_authoring COMMAND RTSReferenceAuthoringTests)
    add_executable(RTSEditorTests tests/editor_tests.cpp)
    target_link_libraries(RTSEditorTests PRIVATE RTSAuthoring)
    add_test(NAME editor COMMAND RTSEditorTests)
    add_executable(RTSSceneTests tests/scene_tests.cpp)
    target_link_libraries(RTSSceneTests PRIVATE RTSAuthoring)
    add_test(NAME scene COMMAND RTSSceneTests)

    add_executable(RTSSpatialTests tests/spatial_tests.cpp)
    target_link_libraries(RTSSpatialTests PRIVATE RTSSimulation)
    add_test(NAME spatial COMMAND RTSSpatialTests)

    add_executable(RTSVisionTests tests/vision_tests.cpp)
    target_link_libraries(RTSVisionTests PRIVATE RTSSimulation)
    add_test(NAME vision COMMAND RTSVisionTests)

    add_executable(RTSHUDTests tests/hud_tests.cpp)
    target_link_libraries(RTSHUDTests PRIVATE RTSSimulation)
    add_test(NAME hud COMMAND RTSHUDTests)
    add_test(NAME sample_project COMMAND RTSProjectTool --validate "${CMAKE_CURRENT_SOURCE_DIR}/../projects/default.rtsproject.json")
    add_test(NAME showcase_project COMMAND RTSProjectTool --validate "${CMAKE_CURRENT_SOURCE_DIR}/../projects/terrain_showcase.rtsproject.json")
    add_test(NAME production_showcase COMMAND RTSProjectTool --validate "${CMAKE_CURRENT_SOURCE_DIR}/../projects/production_showcase.rtsproject.json")
    add_test(NAME effects_showcase COMMAND RTSProjectTool --validate "${CMAKE_CURRENT_SOURCE_DIR}/../projects/effects_showcase.rtsproject.json")
    add_test(NAME diagnostics_showcase COMMAND RTSProjectTool --validate "${CMAKE_CURRENT_SOURCE_DIR}/../projects/trigger_diagnostics_showcase.rtsproject.json")
    add_executable(RTSModdedProjectTests tests/modded_project_tests.cpp)
    target_link_libraries(RTSModdedProjectTests PRIVATE RTSSimulation)
    add_test(NAME modded_project COMMAND RTSModdedProjectTests)
    add_executable(RTSInstanceOverrideTests tests/instance_override_tests.cpp)
    target_link_libraries(RTSInstanceOverrideTests PRIVATE RTSAuthoring)
    add_test(NAME instance_overrides COMMAND RTSInstanceOverrideTests)
    add_executable(RTSTriggerAuthoringTests tests/trigger_authoring_tests.cpp)
    target_link_libraries(RTSTriggerAuthoringTests PRIVATE RTSAuthoring)
    add_test(NAME trigger_authoring COMMAND RTSTriggerAuthoringTests)
    add_executable(RTSTriggerTraceTests tests/trigger_trace_tests.cpp)
    target_link_libraries(RTSTriggerTraceTests PRIVATE RTSSimulation)
    add_test(NAME trigger_trace COMMAND RTSTriggerTraceTests)
    add_executable(RTSTriggerTests tests/trigger_tests.cpp)
    target_link_libraries(RTSTriggerTests PRIVATE RTSSimulation)
    add_test(NAME triggers COMMAND RTSTriggerTests)

    # Preserve assert() checks even when producing optimized Release bundles.
    foreach(test_target IN ITEMS RTSNativeMenuTests RTSNavigationTests RTSDynamicNavigationTests RTSSimulationTests RTSOrdersTests RTSCombatResolutionTests RTSFormationTests RTSUnitDataTests RTSChaseTests RTSTerrainTests RTSCatalogTests RTSCatalogRenameTests RTSCatalogDeletionTests RTSProductionCatalogTests RTSProductionSpawnTests RTSProjectTests RTSEditorTests RTSSceneTests RTSHUDTests RTSVisionTests RTSSpatialTests RTSModdedProjectTests RTSInstanceOverrideTests RTSTriggerTests RTSTriggerAuthoringTests RTSReferenceAuthoringTests RTSTriggerTraceTests)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE $<$<CONFIG:Release>:/UNDEBUG>)
        else()
            target_compile_options(${test_target} PRIVATE $<$<CONFIG:Release>:-UNDEBUG>)
        endif()
    endforeach()
endif()

add_executable(RTSProjectTool "../tools/rts_project_tool.cpp")
target_link_libraries(RTSProjectTool PRIVATE RTSSimulation)

if(RTS_BUILD_DEMO)
    find_package(glfw3 QUIET)
    if(NOT TARGET glfw)
        include(FetchContent)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE) # portable game, not GLFW SDK
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4)
        FetchContent_MakeAvailable(glfw)
    endif()
    find_package(OpenGL REQUIRED)
    include(FetchContent)
    # Pinned docking-enabled Dear ImGui, cross-platform GLFW/OpenGL3 backends.
    # Core game/simulation libraries remain independent of the GUI library.
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b-docking
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(imgui)
    add_library(RTSCreatorGui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")
    target_include_directories(RTSCreatorGui PUBLIC
        "${imgui_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}/backends"
        "${imgui_SOURCE_DIR}/misc/cpp")
    target_link_libraries(RTSCreatorGui PUBLIC glfw OpenGL::GL)
    add_executable(RTSCameraTerrain
        main_entry_point_render_loop.cpp
        opengl_3_3_core_function_loader_implementation.cpp
        rts_camera_rig_implementation.cpp
        selection_manager_implementation.cpp)
    target_link_libraries(RTSCameraTerrain PRIVATE glfw OpenGL::GL RTSSimulation RTSCreatorGui)
    target_compile_definitions(RTSCameraTerrain PRIVATE
        RTS_SOURCE_COMMIT="${RTS_SOURCE_COMMIT}"
        RTS_FEATURE_CREATOR_GUI=1 RTS_FEATURE_FOG_OF_WAR=1
        RTS_FEATURE_GAMEPLAY_HUD=1 RTS_FEATURE_NATIVE_PAUSE_MENU=1
        RTS_FEATURE_VISUAL_ECA_EDITOR=1 RTS_FEATURE_TRIGGER_TRACE_MONITOR=1)
    if(WIN32)
        target_link_libraries(RTSCameraTerrain PRIVATE dbghelp)
    endif()
    if(MSVC)
        # Keep symbols for native crash dump analysis in Release binaries.
        target_link_options(RTSCameraTerrain PRIVATE /DEBUG:FULL)
        install(FILES "$<TARGET_PDB_FILE:RTSCameraTerrain>"
                DESTINATION "symbols" OPTIONAL)
    endif()
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

# Installable layout is intentionally identical on Windows and Linux:
# exe/launcher files beside projects/, docs/, and build manifest.
install(TARGETS RTSProjectTool RUNTIME DESTINATION ".")
if(RTS_BUILD_DEMO)
    install(TARGETS RTSCameraTerrain RUNTIME DESTINATION ".")
endif()
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../projects/"
        DESTINATION "projects" FILES_MATCHING PATTERN "*.rtsproject.json")
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/START_HERE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE"
    DESTINATION ".")
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../docs/AUTHORING.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/../docs/SC2_FEATURE_MATRIX.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/../docs/BUILD_ARTIFACTS.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/../docs/CREATOR_ARCHITECTURE.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/../docs/TRIGGER_EDITOR.md"
    DESTINATION "docs")
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/PICOJSON_LICENSE"
    DESTINATION "licenses")
if(RTS_BUILD_DEMO)
    install(FILES "${imgui_SOURCE_DIR}/LICENSE.txt"
            DESTINATION "licenses" RENAME "IMGUI_LICENSE")
endif()
if(WIN32)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/run-game.bat"
        "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/run-editor.bat"
        "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/run-production-lab.bat"
        DESTINATION ".")
elseif(UNIX)
    install(PROGRAMS
        "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/run-game.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/run-editor.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/run-production-lab.sh"
        DESTINATION ".")
endif()
