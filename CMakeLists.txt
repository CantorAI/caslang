# CMakeList.txt : CMake project for CasLang Package
#
cmake_minimum_required (VERSION 3.10)

project ("caslang")
cmake_policy(SET CMP0091 NEW)

# Option to build Galaxy Filter
option(BUILD_GALAXY_FILTER "Build Galaxy Filter" OFF)

# Using local xlang_core for API
include_directories("xlang_core")
include_directories(.)
include_directories(log)
include_directories(core)

if(BUILD_GALAXY_FILTER)
    add_definitions(-DBUILD_GALAXY_FILTER)
    include_directories("../Galaxy/common")
    include_directories("../Galaxy/log")
    include_directories("../Galaxy/ThirdParty")
    include_directories(filter)
endif()

FILE(GLOB XLANG_API_SRC "xlang_core/value.cpp")
file(GLOB Log_SRC "log/*.cpp")
# Core sources
FILE(GLOB CORE_SRC "core/*.cpp")

set(Main_SRC ${CORE_SRC})

if(BUILD_GALAXY_FILTER)
    FILE(GLOB FILTER_SRC "filter/*.cpp")
    FILE(GLOB Common_SRC "../Galaxy/common/*.cpp" "../Galaxy/log/*.cpp")
    list(APPEND Main_SRC ${FILTER_SRC} ${Common_SRC} )
endif()

# Create the library
add_library(${PROJECT_NAME} 
    STATIC
    ${XLANG_API_SRC}
    ${Log_SRC}
    ${Main_SRC}
)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CompilerFlags
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
    foreach(CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
    
    target_compile_options(${PROJECT_NAME} PRIVATE 
                          /EHsc       # Enable C++ exceptions
                          /D_CRT_SECURE_NO_WARNINGS
    )
endif()

# Touch to trigger re-glob
