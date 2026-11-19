/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#ifndef mitkCESTOnnxConfig_h
#define mitkCESTOnnxConfig_h

#include <MitkCESTOnnxExports.h>

#include <cstddef>
#include <string>
#include <vector>

namespace mitk
{
  /**
   * \brief One output map of a CEST ONNX network as described by an .ini file.
   *
   * The DICOM value of a map is value * Scale + Bias.
   */
  struct MITKCESTONNX_EXPORT CESTOnnxContrast
  {
    /** Name of the map, taken from the [Contrasts] section. */
    std::string Name;
    /** Column of the network output that holds this map. */
    std::size_t AmplitudeId = 0;
    double Scale = 1.0;
    double Bias = 0.0;
  };

  /**
   * \brief Settings of a CEST ONNX network read from an .ini file.
   *
   * Only the "network does everything" workflow is supported: the network
   * receives all volumes after ColShift and normalizes them internally, so
   * normMethod must be 0 and B1 correction must be disabled.
   */
  struct MITKCESTONNX_EXPORT CESTOnnxConfig
  {
    /** Maps with Compute = 1 in the order of the [Contrasts] section. */
    std::vector<CESTOnnxContrast> Contrasts;
    /** Number of leading volumes that are not passed to the network. */
    std::size_t ColShift = 0;
    /** Number of volumes the protocol has in total (0 if not given). */
    std::size_t NumTotalOffsets = 0;

    /**
     * \brief Read the settings from an .ini file.
     * \throw mitk::Exception if the file cannot be read, a required entry is
     * missing or malformed, or the file asks for a workflow that is not supported.
     */
    static CESTOnnxConfig Load(const std::string& path);

    /** \brief Same as Load(), but parses the given text. */
    static CESTOnnxConfig Parse(const std::string& text);
  };
}

#endif
