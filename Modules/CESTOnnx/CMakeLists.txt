mitk_create_module(
  DEPENDS PUBLIC MitkCore
  PACKAGE_DEPENDS PRIVATE onnxruntime
)

if(TARGET ${MODULE_TARGET} AND WIN32)
  # Windows ships an older onnxruntime.dll in System32, which wins over PATH.
  # The copy next to the executables is found first.
  add_custom_command(TARGET ${MODULE_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:onnxruntime::onnxruntime>
      ${MITK_CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>
  )
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
