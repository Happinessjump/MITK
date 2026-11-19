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
   * "o: <ppm>" lines) and the Pulseq sequence file (.seq, only the offsets from
   * the field "offsets_ppm" of the definitions). The format is chosen by the
   * file extension; the offsets are the only values the .seq file provides.
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
   * \brief Read one CEST protocol file (.ini, .log or .seq).
   * \return The parsed information, or no value if the file cannot be read or contains no offsets.
   */
  std::optional<CESTSidecarInfo> ReadCESTSidecarFile(const std::string& file);

  /**
   * \brief Search the directory of the given file for CEST protocol files.
   *
   * All files that contain exactly timeStepCount offsets are merged, missing
   * values are filled from the lower ranked files (name matching the image
   * first, then .ini). If these files contain different offsets, none is
   * used, as the choice is left to the user.
   *
   * \param imageFile Path of the image or of one file of the image series.
   * \param timeStepCount Expected number of offsets. 0 accepts any count.
   * \return The merged information, or no value if no matching file exists.
   */
  std::optional<CESTSidecarInfo> ReadCESTSidecar(const std::string& imageFile, std::size_t timeStepCount);

  /** \brief Key of the reader option that holds the protocol file path chosen by the user. */
  std::string CESTProtocolFileOptionName();

  /** \brief File dialog filter of the protocol file option. */
  std::string CESTProtocolFileOptionFilter();

  /**
   * \brief Protocol file chosen by the user or, if none was chosen, the one found next to the image.
   *
   * \param imageFile Path of the image or of one file of the image series.
   * \param timeStepCount Expected number of offsets.
   * \param chosenFile Path from the reader option; empty to search next to the image.
   * \throw mitk::Exception if a chosen file does not contain timeStepCount offsets.
   */
  std::optional<CESTSidecarInfo> ResolveCESTSidecar(const std::string& imageFile, std::size_t timeStepCount, const std::string& chosenFile);

  /**
   * \brief Offsets 1 to count, used if the data provides no offsets. Logs a warning.
   */
  std::vector<double> SequentialCESTOffsets(std::size_t count);

  /**
   * \brief Set B1 amplitude, pulse duration, duty cycle and frequency of the image.
   *
   * Properties that are already set on the image are kept.
   */
  void ApplyCESTSidecarParameters(Image* image, const CESTSidecarInfo& info);
}

#endif
