# CMakeList.txt : CMake project for CasLang Package
#
cmake_minimum_required (VERSION 3.10)

project ("caslang")

# Option to build Galaxy Filter
option(BUILD_GALAXY_FILTER "Build Galaxy Filter" ON)

# Using relative path to xlang/Api as per sqlite reference
include_directories("../xlang/Api")
include_directories(.)
include_directories(core)

if(BUILD_GALAXY_FILTER)
    add_definitions(-DBUILD_GALAXY_FILTER)
    include_directories("../Galaxy/common")
    include_directories("../Galaxy/log")
    include_directories("../Galaxy/ThirdParty")
    include_directories(filter)
endif()

FILE(GLOB XLANG_API_SRC "../xlang/Api/value.cpp")

# Core sources
FILE(GLOB CORE_SRC "core/*.cpp")

set(Main_SRC ${CORE_SRC} main.cpp)

if(BUILD_GALAXY_FILTER)
    FILE(GLOB FILTER_SRC "filter/*.cpp")
    FILE(GLOB Common_SRC "../Galaxy/common/*.cpp" "../Galaxy/log/*.cpp")
    list(APPEND Main_SRC ${FILTER_SRC} ${Common_SRC})
endif()

# Create the library
add_library(${PROJECT_NAME} 
    SHARED
    ${XLANG_API_SRC}
    ${Main_SRC}
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE 
                          /EHsc       # Enable C++ exceptions
                          /D_CRT_SECURE_NO_WARNINGS
    )
endif()

# Touch to trigger re-glob
