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
      include/fiat/emit.cppm
      include/fiat/templates.cppm
      include/fiat/splice.cppm
      include/fiat/kernel.cppm)

target_compile_features(fiat PUBLIC cxx_std_26)
set_target_properties(fiat PROPERTIES CXX_MODULE_STD ON)
vkpp_enable_warnings(fiat)

find_program(FIAT_SPIRV_AS spirv-as REQUIRED HINTS "$ENV{VULKAN_SDK}/Bin")
find_program(FIAT_SPIRV_VAL spirv-val REQUIRED HINTS "$ENV{VULKAN_SDK}/Bin")

set(FIAT_TEMPLATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/templates")
set(FIAT_TEMPLATE_OUTPUTS)
foreach(ELEMENT IN ITEMS float int uint)
  set(source "${FIAT_TEMPLATE_DIR}/transform_${ELEMENT}.spvasm")
  set(binary "${FIAT_TEMPLATE_DIR}/transform_${ELEMENT}.spv")
  configure_file(templates/transform.spvasm.in "${source}" @ONLY)
  add_custom_command(
    OUTPUT "${binary}"
    COMMAND "${FIAT_SPIRV_AS}" --target-env spv1.6 -o "${binary}" "${source}"
    COMMAND "${FIAT_SPIRV_VAL}" --target-env spv1.6 "${binary}"
    DEPENDS "${source}"
    VERBATIM)
  list(APPEND FIAT_TEMPLATE_OUTPUTS "${binary}")

  set(reduce_source "${FIAT_TEMPLATE_DIR}/reduce_${ELEMENT}.spvasm")
  set(reduce_binary "${FIAT_TEMPLATE_DIR}/reduce_${ELEMENT}.spv")
  configure_file(templates/reduce.spvasm.in "${reduce_source}" @ONLY)
  add_custom_command(
    OUTPUT "${reduce_binary}"
    COMMAND "${FIAT_SPIRV_AS}" --target-env spv1.6 -o "${reduce_binary}" "${reduce_source}"
    COMMAND "${FIAT_SPIRV_VAL}" --target-env spv1.6 "${reduce_binary}"
    DEPENDS "${reduce_source}"
    VERBATIM)
  list(APPEND FIAT_TEMPLATE_OUTPUTS "${reduce_binary}")
endforeach()
add_custom_target(fiat_templates DEPENDS ${FIAT_TEMPLATE_OUTPUTS})
add_dependencies(fiat fiat_templates)

target_compile_options(fiat PRIVATE "--embed-dir=${FIAT_TEMPLATE_DIR}")
set_source_files_properties(
  include/fiat/templates.cppm PROPERTIES
  OBJECT_DEPENDS
  "${FIAT_TEMPLATE_OUTPUTS}")

option(FIAT_BUILD_EXAMPLES "Build fiat examples" ${PROJECT_IS_TOP_LEVEL})
if(FIAT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()