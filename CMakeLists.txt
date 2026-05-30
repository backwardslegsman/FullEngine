cmake_minimum_required(VERSION 3.21)

project(FullRenderer
    VERSION 0.1.0
    DESCRIPTION "Foundation skeleton for an embeddable C++ renderer"
    LANGUAGES CXX
)

option(FULL_RENDERER_BUILD_SAMPLE "Build the sample application placeholder." ON)
option(FULL_RENDERER_BUILD_TESTS "Build CPU-side renderer tests." ON)
option(FULL_RENDERER_ENABLE_BGFX "Build the renderer with the real bgfx backend." ON)
option(FULL_RENDERER_ENABLE_DEBUG_UI "Build the SDL3 sample with Dear ImGui debug UI support." OFF)
option(FULL_RENDERER_ENABLE_SHADER_COMPILE "Add the bgfx shader compilation target used by the sample app." ON)
option(FULL_RENDERER_WARNINGS_AS_ERRORS "Treat renderer library compiler warnings as errors." OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(FULL_RENDERER_ENABLE_BGFX)
    find_package(bgfx CONFIG REQUIRED)

    set(FULL_RENDERER_BGFX_TARGETS "")
    foreach(candidate IN ITEMS bgfx::bx bgfx::bgfx bgfx::bimg bgfx::bimg_decode unofficial::bgfx::bgfx bgfx)
        if(TARGET ${candidate})
            list(APPEND FULL_RENDERER_BGFX_TARGETS ${candidate})
        endif()
    endforeach()

    if(NOT FULL_RENDERER_BGFX_TARGETS)
        message(FATAL_ERROR "The bgfx package was found, but no supported CMake target was exported.")
    endif()

    if(FULL_RENDERER_BUILD_SAMPLE AND FULL_RENDERER_ENABLE_SHADER_COMPILE AND NOT TARGET bgfx::shaderc)
        message(FATAL_ERROR "The bgfx shaderc target is required. Install bgfx host tools through vcpkg.")
    endif()
endif()

if(FULL_RENDERER_BUILD_SAMPLE AND NOT FULL_RENDERER_ENABLE_SHADER_COMPILE)
    message(FATAL_ERROR "full_renderer_sample currently requires FULL_RENDERER_ENABLE_SHADER_COMPILE=ON so it can run from deterministic build-local shader outputs.")
endif()

add_library(full_renderer STATIC
    src/renderer/bgfx/BgfxRenderDevice.cpp
    src/renderer/bgfx/BgfxRenderDevice.hpp
    src/renderer/animation/AnimationSystem.cpp
    src/renderer/animation/AnimationSystem.hpp
    src/renderer/debug/CsmDebug.cpp
    src/renderer/debug/CsmDebug.hpp
    src/renderer/debug/FrameBudget.cpp
    src/renderer/debug/FrameBudget.hpp
    src/renderer/debug/LongSessionChurn.cpp
    src/renderer/debug/LongSessionChurn.hpp
    src/renderer/debug/TerrainDebug.cpp
    src/renderer/debug/TerrainDebug.hpp
    src/renderer/debug/Validation.cpp
    src/renderer/debug/Validation.hpp
    src/renderer/core/RenderDevice.hpp
    src/renderer/core/Renderer.cpp
    src/renderer/core/Renderer.hpp
    src/renderer/public/full_renderer/Handles.hpp
    src/renderer/public/full_renderer/Animation.hpp
    src/renderer/public/full_renderer/ColorGrading.hpp
    src/renderer/public/full_renderer/Fade.hpp
    src/renderer/public/full_renderer/Renderer.hpp
    src/renderer/public/full_renderer/Terrain.hpp
    src/renderer/resources/AssetContracts.cpp
    src/renderer/resources/AssetContracts.hpp
    src/renderer/resources/ResourceLifetime.cpp
    src/renderer/resources/ResourceLifetime.hpp
    src/renderer/scene/ColorGrading.cpp
    src/renderer/scene/ColorGrading.hpp
    src/renderer/scene/CullingDiagnostics.cpp
    src/renderer/scene/CullingDiagnostics.hpp
    src/renderer/scene/Environment.cpp
    src/renderer/scene/Environment.hpp
    src/renderer/scene/Decal.cpp
    src/renderer/scene/Decal.hpp
    src/renderer/scene/Fade.cpp
    src/renderer/scene/Fade.hpp
    src/renderer/scene/Frustum.cpp
    src/renderer/scene/Frustum.hpp
    src/renderer/scene/MaterialPolicy.cpp
    src/renderer/scene/MaterialPolicy.hpp
    src/renderer/scene/Math.hpp
    src/renderer/scene/Particle.cpp
    src/renderer/scene/Particle.hpp
    src/renderer/scene/PostPass.cpp
    src/renderer/scene/PostPass.hpp
    src/renderer/scene/Shadow.cpp
    src/renderer/scene/Shadow.hpp
    src/renderer/scene/Ssao.cpp
    src/renderer/scene/Ssao.hpp
    src/renderer/scene/Weather.cpp
    src/renderer/scene/Weather.hpp
    src/renderer/terrain/TerrainCulling.hpp
    src/renderer/terrain/TerrainGridMesh.cpp
    src/renderer/terrain/TerrainGridMesh.hpp
    src/renderer/terrain/TerrainLod.hpp
    src/renderer/terrain/TerrainSystem.cpp
    src/renderer/terrain/TerrainSystem.hpp
)

add_library(FullRenderer::full_renderer ALIAS full_renderer)

add_library(full_engine STATIC
    src/engine/core/Engine.cpp
    src/engine/core/Engine.hpp
    src/engine/renderer_integration/ChunkTerrainHandleMap.cpp
    src/engine/renderer_integration/ChunkTerrainHandleMap.hpp
    src/engine/renderer_integration/RenderSpace.cpp
    src/engine/renderer_integration/RenderSpace.hpp
    src/engine/renderer_integration/RendererHost.cpp
    src/engine/renderer_integration/RendererHost.hpp
    src/engine/renderer_integration/TerrainDescriptorBuilder.cpp
    src/engine/renderer_integration/TerrainDescriptorBuilder.hpp
    src/engine/renderer_integration/TerrainLifecyclePlan.cpp
    src/engine/renderer_integration/TerrainLifecyclePlan.hpp
    src/engine/renderer_integration/TerrainRendererCommands.cpp
    src/engine/renderer_integration/TerrainRendererCommands.hpp
    src/engine/renderer_integration/TerrainResourceCatalog.cpp
    src/engine/renderer_integration/TerrainResourceCatalog.hpp
    src/engine/renderer_integration/TerrainRenderPrep.cpp
    src/engine/renderer_integration/TerrainRenderPrep.hpp
    src/engine/renderer_integration/WorldRenderSnapshot.cpp
    src/engine/renderer_integration/WorldRenderSnapshot.hpp
    src/engine/world/WorldChunkRegistry.cpp
    src/engine/world/WorldChunkRegistry.hpp
    src/engine/world/WorldOrigin.cpp
    src/engine/world/WorldOrigin.hpp
)

add_library(full_renderer_engine_bridge STATIC EXCLUDE_FROM_ALL
    src/engine_bridge/StreamingSeam.cpp
    src/engine_bridge/StreamingSeam.hpp
)

target_compile_features(full_renderer PUBLIC cxx_std_17)
set_target_properties(full_renderer PROPERTIES
    CXX_EXTENSIONS OFF
)

target_compile_features(full_renderer_engine_bridge PUBLIC cxx_std_17)
set_target_properties(full_renderer_engine_bridge PROPERTIES
    CXX_EXTENSIONS OFF
)

target_compile_features(full_engine PUBLIC cxx_std_17)
set_target_properties(full_engine PROPERTIES
    CXX_EXTENSIONS OFF
)

target_include_directories(full_renderer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/public>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(full_renderer_engine_bridge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(full_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(full_engine
    PUBLIC
        FullRenderer::full_renderer
)

target_link_libraries(full_renderer_engine_bridge
    PUBLIC
        FullRenderer::full_renderer
)

if(FULL_RENDERER_ENABLE_BGFX)
    target_compile_definitions(full_renderer PRIVATE FULL_RENDERER_ENABLE_BGFX=1)
    target_link_libraries(full_renderer PRIVATE ${FULL_RENDERER_BGFX_TARGETS})
else()
    target_compile_definitions(full_renderer PRIVATE FULL_RENDERER_ENABLE_BGFX=0)
endif()

if(MSVC)
    target_compile_options(full_renderer PRIVATE /W4 /permissive-)
    target_compile_options(full_engine PRIVATE /W4 /permissive-)
    target_compile_options(full_renderer_engine_bridge PRIVATE /W4 /permissive-)
    if(FULL_RENDERER_WARNINGS_AS_ERRORS)
        target_compile_options(full_renderer PRIVATE /WX)
        target_compile_options(full_engine PRIVATE /WX)
        target_compile_options(full_renderer_engine_bridge PRIVATE /WX)
    endif()
else()
    target_compile_options(full_renderer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(full_engine PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(full_renderer_engine_bridge PRIVATE -Wall -Wextra -Wpedantic)
    if(FULL_RENDERER_WARNINGS_AS_ERRORS)
        target_compile_options(full_renderer PRIVATE -Werror)
        target_compile_options(full_engine PRIVATE -Werror)
        target_compile_options(full_renderer_engine_bridge PRIVATE -Werror)
    endif()
endif()

set(FULL_RENDERER_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/FullRenderer"
    CACHE STRING
    "Install destination for FullRenderer CMake package files."
)
set(FULL_RENDERER_PACKAGE_USES_BGFX ${FULL_RENDERER_ENABLE_BGFX})

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FullRendererConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/FullRendererConfig.cmake"
    INSTALL_DESTINATION "${FULL_RENDERER_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/FullRendererConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(TARGETS full_renderer
    EXPORT FullRendererTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/public/full_renderer"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT FullRendererTargets
    NAMESPACE FullRenderer::
    FILE FullRendererTargets.cmake
    DESTINATION "${FULL_RENDERER_INSTALL_CMAKEDIR}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/FullRendererConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FullRendererConfigVersion.cmake"
    DESTINATION "${FULL_RENDERER_INSTALL_CMAKEDIR}"
)

if(FULL_RENDERER_BUILD_SAMPLE)
    if(NOT FULL_RENDERER_ENABLE_BGFX)
        message(FATAL_ERROR "full_renderer_sample requires FULL_RENDERER_ENABLE_BGFX=ON.")
    endif()

    find_package(SDL3 CONFIG REQUIRED)

    set(FULL_RENDERER_SDL3_TARGET "")
    foreach(candidate IN ITEMS SDL3::SDL3 SDL3::SDL3-shared SDL3::SDL3-static)
        if(TARGET ${candidate})
            set(FULL_RENDERER_SDL3_TARGET ${candidate})
            break()
        endif()
    endforeach()

    if(NOT FULL_RENDERER_SDL3_TARGET)
        message(FATAL_ERROR "The SDL3 package was found, but no supported CMake target was exported.")
    endif()

    add_executable(full_renderer_sample
        src/app/main.cpp
    )
    target_compile_definitions(full_renderer_sample PRIVATE FULL_RENDERER_ENABLE_BGFX=1)

    if(FULL_RENDERER_ENABLE_DEBUG_UI)
        find_package(imgui CONFIG REQUIRED)

        set(FULL_RENDERER_IMGUI_TARGET "")
        foreach(candidate IN ITEMS imgui::imgui imgui)
            if(TARGET ${candidate})
                set(FULL_RENDERER_IMGUI_TARGET ${candidate})
                break()
            endif()
        endforeach()

        if(NOT FULL_RENDERER_IMGUI_TARGET)
            message(FATAL_ERROR "The imgui package was found, but no supported CMake target was exported.")
        endif()

        target_sources(full_renderer_sample
            PRIVATE
                src/renderer/bgfx/ImguiBgfxRenderer.cpp
                src/renderer/bgfx/ImguiBgfxRenderer.hpp
        )
        target_compile_definitions(full_renderer_sample PRIVATE FULL_RENDERER_ENABLE_DEBUG_UI=1)
        target_link_libraries(full_renderer_sample
            PRIVATE
                ${FULL_RENDERER_IMGUI_TARGET}
                ${FULL_RENDERER_BGFX_TARGETS}
        )
    else()
        target_compile_definitions(full_renderer_sample PRIVATE FULL_RENDERER_ENABLE_DEBUG_UI=0)
    endif()

    set(FULL_RENDERER_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
    set(FULL_RENDERER_SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders/dx11)
    set(FULL_RENDERER_SHADER_VARYING_DEF ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/varying.def.sc)
    set(FULL_RENDERER_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_mesh.bin)
    set(FULL_RENDERER_INSTANCED_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_mesh_instanced.bin)
    set(FULL_RENDERER_SKINNED_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_skinned_mesh.bin)
    set(FULL_RENDERER_TERRAIN_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_terrain_instanced.bin)
    set(FULL_RENDERER_SKY_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_sky.bin)
    set(FULL_RENDERER_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_mesh.bin)
    set(FULL_RENDERER_SKY_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_sky.bin)
    set(FULL_RENDERER_TERRAIN_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_terrain_splat.bin)
    set(FULL_RENDERER_DEBUG_LINE_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_debug_line.bin)
    set(FULL_RENDERER_DEBUG_LINE_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_debug_line.bin)
    set(FULL_RENDERER_IMGUI_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_imgui.bin)
    set(FULL_RENDERER_IMGUI_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_imgui.bin)
    set(FULL_RENDERER_SHADOW_PREVIEW_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_shadow_preview.bin)
    set(FULL_RENDERER_SHADOW_PREVIEW_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_shadow_preview.bin)
    set(FULL_RENDERER_SELECTION_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_selection_mesh.bin)
    set(FULL_RENDERER_SELECTION_INSTANCED_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_selection_instanced.bin)
    set(FULL_RENDERER_SELECTION_SKINNED_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_selection_skinned.bin)
    set(FULL_RENDERER_SELECTION_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_selection_mask.bin)
    set(FULL_RENDERER_OUTLINE_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_outline_fullscreen.bin)
    set(FULL_RENDERER_OUTLINE_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_outline_composite.bin)
    set(FULL_RENDERER_SSAO_DEPTH_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_ssao_depth_mesh.bin)
    set(FULL_RENDERER_SSAO_DEPTH_INSTANCED_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_ssao_depth_instanced.bin)
    set(FULL_RENDERER_SSAO_DEPTH_SKINNED_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_ssao_depth_skinned.bin)
    set(FULL_RENDERER_SSAO_DEPTH_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_ssao_depth.bin)
    set(FULL_RENDERER_SSAO_FULLSCREEN_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_ssao_fullscreen.bin)
    set(FULL_RENDERER_SSAO_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_ssao.bin)
    set(FULL_RENDERER_SSAO_BLUR_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_ssao_blur.bin)
    set(FULL_RENDERER_SSAO_COMPOSITE_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_ssao_composite.bin)
    set(FULL_RENDERER_DECAL_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_decal_projected.bin)
    set(FULL_RENDERER_SCENE_PRESENT_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_scene_present.bin)
    set(FULL_RENDERER_COLOR_GRADE_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_color_grade.bin)
    set(FULL_RENDERER_PARTICLE_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_particles.bin)
    set(FULL_RENDERER_PARTICLE_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_particles.bin)
    set(FULL_RENDERER_TERRAIN_SHADOW_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_terrain_shadow_instanced.bin)
    set(FULL_RENDERER_SKINNED_SHADOW_VERTEX_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/vs_skinned_shadow.bin)
    set(FULL_RENDERER_SHADOW_FRAGMENT_SHADER_OUTPUT ${FULL_RENDERER_SHADER_OUTPUT_DIR}/fs_shadow_depth.bin)

    add_custom_command(
        OUTPUT ${FULL_RENDERER_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/vs_mesh.sc
            -o ${FULL_RENDERER_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/vs_mesh.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_mesh"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/fs_mesh.sc
            -o ${FULL_RENDERER_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/fs_mesh.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_mesh"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_INSTANCED_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/vs_mesh_instanced.sc
            -o ${FULL_RENDERER_INSTANCED_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/vs_mesh_instanced.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_mesh_instanced"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SKINNED_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/vs_skinned_mesh.sc
            -o ${FULL_RENDERER_SKINNED_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/vs_skinned_mesh.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_skinned_mesh"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_TERRAIN_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/terrain/vs_terrain_instanced.sc
            -o ${FULL_RENDERER_TERRAIN_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/terrain/vs_terrain_instanced.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_terrain_instanced"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SKY_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/vs_sky.sc
            -o ${FULL_RENDERER_SKY_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/vs_sky.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_sky"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_TERRAIN_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/terrain/fs_terrain_splat.sc
            -o ${FULL_RENDERER_TERRAIN_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/terrain/fs_terrain_splat.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_terrain_splat"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SKY_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/fs_sky.sc
            -o ${FULL_RENDERER_SKY_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/forward/fs_sky.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_sky"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_DEBUG_LINE_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_debug_line.sc
            -o ${FULL_RENDERER_DEBUG_LINE_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_debug_line.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_debug_line"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_DEBUG_LINE_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_debug_line.sc
            -o ${FULL_RENDERER_DEBUG_LINE_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_debug_line.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_debug_line"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_IMGUI_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_imgui.sc
            -o ${FULL_RENDERER_IMGUI_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_imgui.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_imgui"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_IMGUI_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_imgui.sc
            -o ${FULL_RENDERER_IMGUI_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_imgui.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_imgui"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SHADOW_PREVIEW_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_shadow_preview.sc
            -o ${FULL_RENDERER_SHADOW_PREVIEW_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_shadow_preview.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_shadow_preview"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SHADOW_PREVIEW_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_shadow_preview.sc
            -o ${FULL_RENDERER_SHADOW_PREVIEW_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_shadow_preview.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_shadow_preview"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SELECTION_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_selection_mesh.sc
            -o ${FULL_RENDERER_SELECTION_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_selection_mesh.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_selection_mesh"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SELECTION_INSTANCED_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_selection_instanced.sc
            -o ${FULL_RENDERER_SELECTION_INSTANCED_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_selection_instanced.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_selection_instanced"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SELECTION_SKINNED_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_selection_skinned.sc
            -o ${FULL_RENDERER_SELECTION_SKINNED_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_selection_skinned.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_selection_skinned"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SELECTION_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_selection_mask.sc
            -o ${FULL_RENDERER_SELECTION_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_selection_mask.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_selection_mask"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_OUTLINE_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_outline_fullscreen.sc
            -o ${FULL_RENDERER_OUTLINE_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_outline_fullscreen.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_outline_fullscreen"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_OUTLINE_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_outline_composite.sc
            -o ${FULL_RENDERER_OUTLINE_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_outline_composite.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_outline_composite"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SSAO_DEPTH_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_ssao_depth_mesh.sc
            -o ${FULL_RENDERER_SSAO_DEPTH_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_ssao_depth_mesh.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_ssao_depth_mesh"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SSAO_DEPTH_INSTANCED_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_ssao_depth_instanced.sc
            -o ${FULL_RENDERER_SSAO_DEPTH_INSTANCED_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_ssao_depth_instanced.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_ssao_depth_instanced"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SSAO_DEPTH_SKINNED_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_ssao_depth_skinned.sc
            -o ${FULL_RENDERER_SSAO_DEPTH_SKINNED_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_ssao_depth_skinned.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_ssao_depth_skinned"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SSAO_DEPTH_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_ssao_depth.sc
            -o ${FULL_RENDERER_SSAO_DEPTH_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_ssao_depth.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_ssao_depth"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SSAO_FULLSCREEN_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_ssao_fullscreen.sc
            -o ${FULL_RENDERER_SSAO_FULLSCREEN_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_ssao_fullscreen.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_ssao_fullscreen"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SSAO_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_ssao.sc
            -o ${FULL_RENDERER_SSAO_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_ssao.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_ssao"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SSAO_COMPOSITE_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_ssao_composite.sc
            -o ${FULL_RENDERER_SSAO_COMPOSITE_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_ssao_composite.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_ssao_composite"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SSAO_BLUR_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_ssao_blur.sc
            -o ${FULL_RENDERER_SSAO_BLUR_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_ssao_blur.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_ssao_blur"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_TERRAIN_SHADOW_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/shadows/vs_terrain_shadow_instanced.sc
            -o ${FULL_RENDERER_TERRAIN_SHADOW_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/shadows/vs_terrain_shadow_instanced.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_terrain_shadow_instanced"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_DECAL_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_decal_projected.sc
            -o ${FULL_RENDERER_DECAL_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_decal_projected.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_decal_projected"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SCENE_PRESENT_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_scene_present.sc
            -o ${FULL_RENDERER_SCENE_PRESENT_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_scene_present.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_scene_present"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_COLOR_GRADE_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_color_grade.sc
            -o ${FULL_RENDERER_COLOR_GRADE_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_color_grade.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_color_grade"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_PARTICLE_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_particles.sc
            -o ${FULL_RENDERER_PARTICLE_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/vs_particles.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_particles"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_PARTICLE_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_particles.sc
            -o ${FULL_RENDERER_PARTICLE_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/debug/fs_particles.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_particles"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SHADOW_FRAGMENT_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/shadows/fs_shadow_depth.sc
            -o ${FULL_RENDERER_SHADOW_FRAGMENT_SHADER_OUTPUT}
            --type fragment
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/shadows/fs_shadow_depth.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx fragment shader fs_shadow_depth"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FULL_RENDERER_SKINNED_SHADOW_VERTEX_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FULL_RENDERER_SHADER_OUTPUT_DIR}
        COMMAND bgfx::shaderc
            -f ${FULL_RENDERER_SHADER_SOURCE_DIR}/shadows/vs_skinned_shadow.sc
            -o ${FULL_RENDERER_SKINNED_SHADOW_VERTEX_SHADER_OUTPUT}
            --type vertex
            --platform windows
            --profile s_5_0
            --varyingdef ${FULL_RENDERER_SHADER_VARYING_DEF}
            -i ${BGFX_SHADER_INCLUDE_PATH}
            -i ${FULL_RENDERER_SHADER_SOURCE_DIR}
            --Werror
        MAIN_DEPENDENCY ${FULL_RENDERER_SHADER_SOURCE_DIR}/shadows/vs_skinned_shadow.sc
        DEPENDS ${FULL_RENDERER_SHADER_VARYING_DEF}
        COMMENT "Compiling bgfx vertex shader vs_skinned_shadow"
        VERBATIM
    )

    set(FULL_RENDERER_SHADER_TARGET_OUTPUTS
        ${FULL_RENDERER_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_INSTANCED_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SKINNED_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_TERRAIN_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SKY_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_SKY_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_TERRAIN_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_DEBUG_LINE_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_DEBUG_LINE_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_SELECTION_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SELECTION_INSTANCED_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SELECTION_SKINNED_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SELECTION_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_OUTLINE_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_OUTLINE_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_SSAO_DEPTH_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SSAO_DEPTH_INSTANCED_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SSAO_DEPTH_SKINNED_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SSAO_DEPTH_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_SSAO_FULLSCREEN_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SSAO_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_SSAO_BLUR_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_SSAO_COMPOSITE_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_DECAL_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_SCENE_PRESENT_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_COLOR_GRADE_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_PARTICLE_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_PARTICLE_FRAGMENT_SHADER_OUTPUT}
        ${FULL_RENDERER_TERRAIN_SHADOW_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SKINNED_SHADOW_VERTEX_SHADER_OUTPUT}
        ${FULL_RENDERER_SHADOW_FRAGMENT_SHADER_OUTPUT}
    )

    if(FULL_RENDERER_ENABLE_DEBUG_UI)
        list(APPEND FULL_RENDERER_SHADER_TARGET_OUTPUTS
            ${FULL_RENDERER_IMGUI_VERTEX_SHADER_OUTPUT}
            ${FULL_RENDERER_IMGUI_FRAGMENT_SHADER_OUTPUT}
            ${FULL_RENDERER_SHADOW_PREVIEW_VERTEX_SHADER_OUTPUT}
            ${FULL_RENDERER_SHADOW_PREVIEW_FRAGMENT_SHADER_OUTPUT}
        )
    endif()

    add_custom_target(full_renderer_shaders
        DEPENDS ${FULL_RENDERER_SHADER_TARGET_OUTPUTS}
    )

    add_dependencies(full_renderer_sample full_renderer_shaders)

    target_link_libraries(full_renderer_sample
        PRIVATE
            FullRenderer::full_renderer
            full_renderer_engine_bridge
            ${FULL_RENDERER_SDL3_TARGET}
    )
    file(TO_CMAKE_PATH "${FULL_RENDERER_SHADER_OUTPUT_DIR}" FULL_RENDERER_SAMPLE_SHADER_DIR)
    target_compile_definitions(full_renderer_sample
        PRIVATE
            FULL_RENDERER_SAMPLE_SHADER_DIR="${FULL_RENDERER_SAMPLE_SHADER_DIR}"
    )
    target_include_directories(full_renderer_sample
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_target_properties(full_renderer_sample PROPERTIES
        CXX_EXTENSIONS OFF
    )

    add_custom_command(TARGET full_renderer_sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:full_renderer_sample>
            $<TARGET_FILE_DIR:full_renderer_sample>
        COMMAND_EXPAND_LISTS
    )
endif()

include(CTest)

if(BUILD_TESTING AND FULL_RENDERER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
