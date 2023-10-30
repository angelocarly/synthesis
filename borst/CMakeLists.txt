set(
        SOURCE_FILES
        main.cpp
)

set(
        HEADER_FILES
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

include_directories( ${PROJECT_ROOT_DIR} )