cmake_minimum_required(VERSION 3.21)

project(FullRendererConsumerSmoke
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(FullRenderer CONFIG REQUIRED)

add_executable(full_renderer_consumer_smoke
    main.cpp
)

target_link_libraries(full_renderer_consumer_smoke
    PRIVATE
        FullRenderer::full_renderer
)

target_compile_features(full_renderer_consumer_smoke
    PRIVATE
        cxx_std_17
)

set_target_properties(full_renderer_consumer_smoke PROPERTIES
    CXX_EXTENSIONS OFF
)
