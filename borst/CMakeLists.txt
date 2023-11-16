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
    Borst
        ${SOURCE_FILES}
        ${HEADER_FILES}
)

set_property( TARGET Borst PROPERTY CXX_STANDARD 17 )

## Project libraries
target_link_libraries(
        Borst PUBLIC
        Burst
)

include_directories( ${BURST_ROOT_DIR} )

compile_shaders( Borst "${CMAKE_SOURCE_DIR}/resources/" "${CMAKE_CURRENT_BINARY_DIR}/resources/shaders/" "${GLSL_SOURCE_FILES}" )
