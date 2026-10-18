cmake_minimum_required(VERSION 4.0)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")

project(fiat LANGUAGES CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectWarnings.cmake")

add_library(fiat STATIC)
add_library(fiat::fiat ALIAS fiat)

target_sources(fiat
  PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
      include/fiat/fiat.cppm
      include/fiat/spirv.cppm
      include/fiat/trace.cppm
      include/fiat/emit.cppm)

target_compile_features(fiat PUBLIC cxx_std_26)
set_target_properties(fiat PROPERTIES CXX_MODULE_STD ON)
vkpp_enable_warnings(fiat)

option(FIAT_BUILD_EXAMPLES "Build fiat examples" ${PROJECT_IS_TOP_LEVEL})
if(FIAT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()