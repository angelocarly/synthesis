set(
    HEADER_FILES
        TestPresenter.h
)

set(
    SOURCE_FILES
        Source/TestPresenter.cpp
        main.cpp
)

set(
    GLSL_SOURCE_FILES
        shaders/Sampler.frag
        shaders/Compute.comp
)

add_executable(
    Synthesis
        ${SOURCE_FILES}
        ${HEADER_FILES}
)

set_property( TARGET Synthesis PROPERTY CXX_STANDARD 17 )

## Project libraries
target_link_libraries(
        Synthesis PUBLIC
        Burst
        Imgui
)

include_directories( ${BURST_ROOT_DIR} )

compile_shaders( Synthesis "${CMAKE_SOURCE_DIR}/resources/" "${CMAKE_CURRENT_BINARY_DIR}/resources/shaders/" "${GLSL_SOURCE_FILES}" )
