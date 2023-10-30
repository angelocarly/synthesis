cmake_minimum_required( VERSION 3.3 )
project( Borst VERSION 0.1.0 )

set( PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )

set( LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib" )

add_subdirectory( lib )
include_directories( lib/burst )

add_subdirectory( borst )