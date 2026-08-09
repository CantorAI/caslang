# Copyright (C) 2026 CantorAI Inc.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# CMakeList.txt : CMake project for CasLang Package
#
cmake_minimum_required(VERSION 3.20)

project(caslang VERSION 0.3.0 LANGUAGES CXX)
cmake_policy(SET CMP0091 NEW)

set(_caslang_build_cli_default OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_caslang_build_cli_default ON)
endif()
option(CASLANG_BUILD_CLI "Build the CasLang command-line executable" ${_caslang_build_cli_default})
option(CASLANG_BUILD_COMPAT "Expose build-tree-only legacy core headers" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB Log_SRC "log/*.cpp")
# Core sources
FILE(GLOB CORE_SRC "core/*.cpp")

add_library(caslang STATIC ${Log_SRC} ${CORE_SRC} src/CasLang.cpp)
add_library(CasLang::caslang ALIAS caslang)
target_include_directories(caslang PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_include_directories(caslang PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/log
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/include)

if(CASLANG_BUILD_COMPAT)
    add_library(caslang_compat INTERFACE)
    add_library(CasLang::compat ALIAS caslang_compat)
    target_include_directories(caslang_compat INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/log
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/include)
    target_link_libraries(caslang_compat INTERFACE CasLang::caslang)
endif()

if(CASLANG_BUILD_CLI)
    add_executable(caslang_cli cli/main.cpp)
    set_target_properties(caslang_cli PROPERTIES OUTPUT_NAME caslang)
    target_include_directories(caslang_cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/include)
    target_link_libraries(caslang_cli PRIVATE CasLang::caslang)
endif()

if(MSVC)
    # Keep Debug on MTd so CasLang remains debuggable while Release uses MT.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_property(TARGET caslang PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    if(CASLANG_BUILD_CLI)
        set_property(TARGET caslang_cli PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    set(CompilerFlags
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
    foreach(CompilerFlag ${CompilerFlags})
        string(REPLACE "/MDd" "/MTd" ${CompilerFlag} "${${CompilerFlag}}")
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()

    target_compile_options(caslang PRIVATE
                          /utf-8
                          /EHsc       # Enable C++ exceptions
                          /D_CRT_SECURE_NO_WARNINGS
    )
    target_compile_definitions(caslang PRIVATE
                          _ITERATOR_DEBUG_LEVEL=0
                          RTC_DCHECK_IS_ON=0)
    if(CASLANG_BUILD_COMPAT)
        target_compile_options(caslang_compat INTERFACE /utf-8)
    endif()
endif()

if(MSVC AND CASLANG_BUILD_CLI)
    target_compile_options(caslang_cli PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS caslang
    EXPORT caslangTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(CASLANG_BUILD_CLI)
    install(TARGETS caslang_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
install(EXPORT caslangTargets
    FILE caslangTargets.cmake
    NAMESPACE CasLang::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/caslang)
configure_package_config_file(
    cmake/caslangConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/caslangConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/caslang)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/caslangConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/caslangConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/caslangConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/caslang)
