file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}" _fiat_spv_output_dir)
string(REPLACE "\\" "/" _fiat_spv_output_dir "${_fiat_spv_output_dir}")
foreach(
  example IN ITEMS
  multiply increment transform scale reduce transform_with scan selection
  compact histogram)
  add_executable(fiat_${example} ${example}.cpp)
  target_link_libraries(fiat_${example} PRIVATE fiat::fiat)
  set_target_properties(fiat_${example} PROPERTIES CXX_MODULE_STD ON)
  vkpp_enable_warnings(fiat_${example})
  target_compile_definitions(fiat_${example} PRIVATE
    FIAT_SPV_OUTPUT_DIR="${_fiat_spv_output_dir}")
endforeach()

add_executable(fiat_struct_probe struct_probe.cpp)
target_link_libraries(fiat_struct_probe PRIVATE fiat::fiat stdc++exp)
set_target_properties(fiat_struct_probe PROPERTIES CXX_MODULE_STD ON)
vkpp_enable_warnings(fiat_struct_probe)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(fiat_struct_probe PRIVATE -freflection)
endif()