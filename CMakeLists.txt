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
      include/fiat/stage.cppm
      include/fiat/graph.cppm
      include/fiat/kernel.cppm
      include/fiat/structs.cppm
      include/fiat/operations.cppm)

target_compile_features(fiat PUBLIC cxx_std_26)
set_target_properties(fiat PROPERTIES CXX_MODULE_STD ON)
vkpp_enable_warnings(fiat)

target_compile_options(fiat PUBLIC $<$<CXX_COMPILER_ID:GNU>:-freflection>)

find_program(FIAT_SPIRV_AS spirv-as REQUIRED HINTS "$ENV{VULKAN_SDK}/Bin")
find_program(FIAT_SPIRV_VAL spirv-val REQUIRED HINTS "$ENV{VULKAN_SDK}/Bin")

set(FIAT_TEMPLATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/templates")
set(FIAT_TEMPLATE_OUTPUTS)
function(fiat_add_template NAME TEMPLATE)
  set(source "${FIAT_TEMPLATE_DIR}/${NAME}.spvasm")
  set(binary "${FIAT_TEMPLATE_DIR}/${NAME}.spv")
  configure_file(templates/${TEMPLATE}.spvasm.in "${source}" @ONLY)
  add_custom_command(
    OUTPUT "${binary}"
    COMMAND "${FIAT_SPIRV_AS}" --target-env spv1.6 -o "${binary}" "${source}"
    COMMAND "${FIAT_SPIRV_VAL}" --target-env spv1.6 "${binary}"
    DEPENDS "${source}"
    VERBATIM)
  list(APPEND FIAT_TEMPLATE_OUTPUTS "${binary}")
  set(FIAT_TEMPLATE_OUTPUTS "${FIAT_TEMPLATE_OUTPUTS}" PARENT_SCOPE)
endfunction()

foreach(ELEMENT IN ITEMS float int uint)
  foreach(TEMPLATE IN ITEMS transform reduce scatter predicate)
    fiat_add_template(${TEMPLATE}_${ELEMENT} ${TEMPLATE})
  endforeach()
  foreach(MODE IN ITEMS inclusive exclusive)
    if(MODE STREQUAL "exclusive")
      set(SCAN_EXCLUSIVE OpConstantTrue)
    else()
      set(SCAN_EXCLUSIVE OpConstantFalse)
    endif()
    fiat_add_template(scan_${MODE}_${ELEMENT} scan)
  endforeach()
endforeach()

foreach(TEMPLATE IN ITEMS histogram tile_histogram stable_scatter)
  fiat_add_template(${TEMPLATE}_uint ${TEMPLATE})
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