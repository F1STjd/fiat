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
      include/fiat/kernel.cppm
      include/fiat/structs.cppm
      include/fiat/ops.cppm)

target_compile_features(fiat PUBLIC cxx_std_26)
set_target_properties(fiat PROPERTIES CXX_MODULE_STD ON)
vkpp_enable_warnings(fiat)

target_compile_options(fiat PUBLIC $<$<CXX_COMPILER_ID:GNU>:-freflection>)

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

  foreach(MODE IN ITEMS inclusive exclusive)
    if(MODE STREQUAL "exclusive")
      set(SCAN_EXCLUSIVE OpConstantTrue)
    else()
      set(SCAN_EXCLUSIVE OpConstantFalse)
    endif()
    set(scan_source "${FIAT_TEMPLATE_DIR}/scan_${MODE}_${ELEMENT}.spvasm")
    set(scan_binary "${FIAT_TEMPLATE_DIR}/scan_${MODE}_${ELEMENT}.spv")
    configure_file(templates/scan.spvasm.in "${scan_source}" @ONLY)
    add_custom_command(
      OUTPUT "${scan_binary}"
      COMMAND "${FIAT_SPIRV_AS}" --target-env spv1.6 -o "${scan_binary}" "${scan_source}"
      COMMAND "${FIAT_SPIRV_VAL}" --target-env spv1.6 "${scan_binary}"
      DEPENDS "${scan_source}"
      VERBATIM)
    list(APPEND FIAT_TEMPLATE_OUTPUTS "${scan_binary}")
  endforeach()

  set(scatter_source "${FIAT_TEMPLATE_DIR}/scatter_${ELEMENT}.spvasm")
  set(scatter_binary "${FIAT_TEMPLATE_DIR}/scatter_${ELEMENT}.spv")
  configure_file(templates/scatter.spvasm.in "${scatter_source}" @ONLY)
  add_custom_command(
    OUTPUT "${scatter_binary}"
    COMMAND "${FIAT_SPIRV_AS}" --target-env spv1.6 -o "${scatter_binary}" "${scatter_source}"
    COMMAND "${FIAT_SPIRV_VAL}" --target-env spv1.6 "${scatter_binary}"
    DEPENDS "${scatter_source}"
    VERBATIM)
  list(APPEND FIAT_TEMPLATE_OUTPUTS "${scatter_binary}")

  foreach(COMPARE IN ITEMS greater greater_equal less less_equal equal not_equal)
    if(ELEMENT STREQUAL "float")
      if(COMPARE STREQUAL "greater")
        set(COMPARE_OP OpFOrdGreaterThan)
      elseif(COMPARE STREQUAL "greater_equal")
        set(COMPARE_OP OpFOrdGreaterThanEqual)
      elseif(COMPARE STREQUAL "less")
        set(COMPARE_OP OpFOrdLessThan)
      elseif(COMPARE STREQUAL "less_equal")
        set(COMPARE_OP OpFOrdLessThanEqual)
      elseif(COMPARE STREQUAL "equal")
        set(COMPARE_OP OpFOrdEqual)
      else()
        set(COMPARE_OP OpFOrdNotEqual)
      endif()
      set(THRESHOLD_FROM_BITS "OpBitcast %float %threshold_bits")
    elseif(ELEMENT STREQUAL "int")
      if(COMPARE STREQUAL "greater")
        set(COMPARE_OP OpSGreaterThan)
      elseif(COMPARE STREQUAL "greater_equal")
        set(COMPARE_OP OpSGreaterThanEqual)
      elseif(COMPARE STREQUAL "less")
        set(COMPARE_OP OpSLessThan)
      elseif(COMPARE STREQUAL "less_equal")
        set(COMPARE_OP OpSLessThanEqual)
      elseif(COMPARE STREQUAL "equal")
        set(COMPARE_OP OpIEqual)
      else()
        set(COMPARE_OP OpINotEqual)
      endif()
      set(THRESHOLD_FROM_BITS "OpBitcast %int %threshold_bits")
    else()
      if(COMPARE STREQUAL "greater")
        set(COMPARE_OP OpUGreaterThan)
      elseif(COMPARE STREQUAL "greater_equal")
        set(COMPARE_OP OpUGreaterThanEqual)
      elseif(COMPARE STREQUAL "less")
        set(COMPARE_OP OpULessThan)
      elseif(COMPARE STREQUAL "less_equal")
        set(COMPARE_OP OpULessThanEqual)
      elseif(COMPARE STREQUAL "equal")
        set(COMPARE_OP OpIEqual)
      else()
        set(COMPARE_OP OpINotEqual)
      endif()
      set(THRESHOLD_FROM_BITS "OpCopyObject %uint %threshold_bits")
    endif()
    set(pred_source "${FIAT_TEMPLATE_DIR}/predicate_${COMPARE}_${ELEMENT}.spvasm")
    set(pred_binary "${FIAT_TEMPLATE_DIR}/predicate_${COMPARE}_${ELEMENT}.spv")
    configure_file(templates/predicate.spvasm.in "${pred_source}" @ONLY)
    add_custom_command(
      OUTPUT "${pred_binary}"
      COMMAND "${FIAT_SPIRV_AS}" --target-env spv1.6 -o "${pred_binary}" "${pred_source}"
      COMMAND "${FIAT_SPIRV_VAL}" --target-env spv1.6 "${pred_binary}"
      DEPENDS "${pred_source}"
      VERBATIM)
    list(APPEND FIAT_TEMPLATE_OUTPUTS "${pred_binary}")
  endforeach()
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