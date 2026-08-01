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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(caslang_nlohmann_json INTERFACE)
target_include_directories(caslang_nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/include)

file(GLOB Log_SRC "log/*.cpp")
# Core sources
FILE(GLOB CORE_SRC "core/*.cpp")

add_library(caslang STATIC ${Log_SRC} ${CORE_SRC} src/CasLang.cpp)
target_include_directories(caslang PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_include_directories(caslang PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/log)
target_link_libraries(caslang PRIVATE caslang_nlohmann_json)

add_executable(caslang_cli cli/main.cpp)
set_target_properties(caslang_cli PROPERTIES OUTPUT_NAME caslang)
target_link_libraries(caslang_cli PRIVATE caslang)
target_link_libraries(caslang_cli PRIVATE caslang_nlohmann_json)

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
    
    target_compile_options(caslang PRIVATE
                          /utf-8
                          /EHsc       # Enable C++ exceptions
                          /D_CRT_SECURE_NO_WARNINGS
    )
endif()

if(MSVC)
    target_compile_options(caslang_cli PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS caslang caslang_cli
    EXPORT caslangTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
