#-----------------------------------------------------------------------------
# onnxruntime
#
# Prebuilt CPU release of ONNX Runtime from the Microsoft.ML.OnnxRuntime
# NuGet package (a plain zip archive). Only Windows x64 is supported.
#-----------------------------------------------------------------------------

set(proj onnxruntime)
set(proj_DEPENDENCIES )

if(MITK_USE_${proj})
  set(${proj}_DEPENDS ${proj})

  if(NOT WIN32)
    message(FATAL_ERROR "The prebuilt ONNX Runtime is only available for Windows. Set MITK_USE_onnxruntime to OFF.")
  endif()

  if(DEFINED ${proj}_DIR AND NOT EXISTS ${${proj}_DIR})
    message(FATAL_ERROR "${proj}_DIR variable is defined but corresponds to non-existing directory!")
  endif()

  if(NOT DEFINED ${proj}_DIR)
    set(_native "<SOURCE_DIR>/runtimes/win-x64/native")

    ExternalProject_Add(${proj}
      URL https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.31.1/microsoft.ml.onnxruntime.1.31.1.nupkg
      URL_HASH SHA256=6871CCBC97F3D5BDB4095A1DCEC33AA78D56F6ED8DE5980EF5FA1D9F2E7C679A
      DOWNLOAD_NAME microsoft.ml.onnxruntime.1.31.1.zip
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory <INSTALL_DIR>/bin <INSTALL_DIR>/lib/cmake/onnxruntime
        COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/build/native/include <INSTALL_DIR>/include/onnxruntime
        COMMAND ${CMAKE_COMMAND} -E copy ${_native}/onnxruntime.dll ${_native}/onnxruntime_providers_shared.dll <INSTALL_DIR>/bin
        COMMAND ${CMAKE_COMMAND} -E copy ${_native}/onnxruntime.lib ${_native}/onnxruntime_providers_shared.lib <INSTALL_DIR>/lib
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/onnxruntimeConfig.cmake <INSTALL_DIR>/lib/cmake/onnxruntime
      DEPENDS ${proj_DEPENDENCIES}
    )

    set(${proj}_DIR ${ep_prefix}/lib/cmake/onnxruntime)
  else()
    mitkMacroEmptyExternalProject(${proj} "${proj_DEPENDENCIES}")
  endif()
endif()
