# ICESat-2 SlideRule Plugin top-level CMake build script

project (ICESAT2 LANGUAGES CXX)

#################
# CMake Options #
#################

set (CMAKE_LEGACY_CYGWIN_WIN32 0) # Squelch a warning when building on Win32/Cygwin
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
cmake_policy(SET CMP0079 NEW) # add link library to target which is not built in this directory
cmake_policy(SET CMP0053 NEW) # simplified variable escape processing (recommended by cmake)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(default_build_type "Release")
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
      set(default_build_type "Debug")
    endif()
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()

###################
# Project Options #
###################

set (CMAKE_SKIP_BUILD_RPATH FALSE)
set (CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set (CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

###################
# ICESat-2 Plugin #
###################

add_library (icesat2 MODULE "")

set_target_properties (icesat2 PROPERTIES OUTPUT_NAME icesat2)
set_target_properties (icesat2 PROPERTIES PREFIX "")

# Version Information #
file(STRINGS ${PROJECT_SOURCE_DIR}/version.txt TGTVER)

# Prefer libraries installed in /usr/local
INCLUDE_DIRECTORIES(/usr/local/include)
LINK_DIRECTORIES(/usr/local/lib)

# Set Environment Variables
set (INSTALLDIR /usr/local CACHE STRING "Installation directory for library and executables")
set (CONFDIR ${INSTALLDIR}/etc/sliderule)
set (RUNTIMEDIR ${CONFDIR} CACHE STRING "Runtime directory for plugins and configuration scripts")

# Compile Definitions #
target_compile_definitions (sliderule PUBLIC BINID="${TGTVER}")
target_compile_definitions (sliderule PUBLIC CONFIGPATH="${RUNTIMEDIR}")

# Source Files #
target_sources(icesat2
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/plugin/icesat2.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/lua_parms.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl03Reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl03Indexer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl06Dispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/UT_Atl06Dispatch.cpp
)

# Include Directories #
target_include_directories (icesat2
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Script Installation #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/indexer.lua
    DESTINATION
        ${CONFDIR}
)

# Plugin Installation #
install (TARGETS icesat2 LIBRARY DESTINATION ${CONFDIR})
