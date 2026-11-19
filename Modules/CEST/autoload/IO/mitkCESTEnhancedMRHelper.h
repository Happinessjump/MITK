/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#ifndef mitkCESTEnhancedMRHelper_h
#define mitkCESTEnhancedMRHelper_h

#include <optional>
#include <string>

namespace mitk
{
  /** \brief Header information of one DICOM file that is needed to assemble a CEST series. */
  struct CESTEnhancedMRFileInfo
  {
    bool isEnhancedMR = false;
    std::string seriesInstanceUID;
    std::string seriesDescription;
    std::string imageComment;
    std::optional<int> instanceNumber;
  };

  /** \brief Scan the header of a DICOM file. Unreadable files yield isEnhancedMR == false. */
  CESTEnhancedMRFileInfo ReadCESTEnhancedMRFileInfo(const std::string& file);

  /**
   * \brief Extract the saturation offset in ppm from the image comment.
   *
   * The Siemens CEST sequence writes "Offset: <value> ppm" into the image comment.
   */
  std::optional<double> ParseCESTOffsetPpm(const std::string& comment);

  /** \brief Extract the saturation amplitude in uT ("B1 Power: <value> uT") from the image comment. */
  std::optional<double> ParseCESTB1Microtesla(const std::string& comment);

  /**
   * \brief Read the scanner frequency of an Enhanced MR file.
   *
   * Enhanced MR objects have no top-level Imaging Frequency (0018,0084). The
   * frequency is stored as Transmit Frequency (0018,9098) in the shared
   * functional groups instead.
   *
   * \return The frequency in MHz, or nothing if the file does not contain it.
   */
  std::optional<double> ReadTransmitFrequencyMHz(const std::string& file);
}

#endif
