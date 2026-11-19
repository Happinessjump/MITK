# Package configuration for the prebuilt ONNX Runtime that OnnxRuntime.cmake
# installs below this file's directory (<prefix>/lib/cmake/onnxruntime).

get_filename_component(_onnxruntime_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT TARGET onnxruntime::onnxruntime)
  add_library(onnxruntime::onnxruntime SHARED IMPORTED)
  set_target_properties(onnxruntime::onnxruntime PROPERTIES
    IMPORTED_LOCATION "${_onnxruntime_prefix}/bin/onnxruntime.dll"
    IMPORTED_IMPLIB "${_onnxruntime_prefix}/lib/onnxruntime.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${_onnxruntime_prefix}/include/onnxruntime"
  )
endif()

unset(_onnxruntime_prefix)
set(onnxruntime_FOUND TRUE)
