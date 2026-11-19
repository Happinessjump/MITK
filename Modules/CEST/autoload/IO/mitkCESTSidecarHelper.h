/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#ifndef mitkCESTSidecarHelper_h
#define mitkCESTSidecarHelper_h

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace mitk
{
  class Image;

  /**
   * \brief CEST acquisition parameters read from a protocol file next to an image.
   *
   * Supported are the Siemens sequence configuration (.ini, section [Offsets]
   * with "Offset[n] = <ppm> <B1 in uT>") and the CEST parameter log (.log,
   * "o: <ppm>" lines). Both are recognized by their content.
   */
  struct CESTSidecarInfo
  {
    std::vector<double> offsetsPpm;
    std::optional<double> b1Microtesla;
    std::optional<double> pulseDurationMs;
    std::optional<double> dutyCyclePercent;
    std::optional<double> frequencyMHz;
  };

  /**
   * \brief Search the directory of the given file for CEST protocol files.
   *
   * Files whose name matches the image name take precedence. All files that
   * contain exactly timeStepCount offsets are merged, missing values are
   * filled from the lower ranked files.
   *
   * \param imageFile Path of the image or of one file of the image series.
   * \param timeStepCount Expected number of offsets. 0 accepts any count.
   * \return The merged information, or no value if no matching file exists.
   */
  std::optional<CESTSidecarInfo> ReadCESTSidecar(const std::string& imageFile, std::size_t timeStepCount);

  /**
   * \brief Set B1 amplitude, pulse duration, duty cycle and frequency of the image.
   *
   * Properties that are already set on the image are kept.
   */
  void ApplyCESTSidecarParameters(Image* image, const CESTSidecarInfo& info);
}

#endif
