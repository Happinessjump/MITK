/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#ifndef mitkCESTOnnxEvaluator_h
#define mitkCESTOnnxEvaluator_h

#include <MitkCESTOnnxExports.h>

#include <mitkCESTOnnxConfig.h>
#include <mitkImage.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mitk
{
  /**
   * \brief Voxel-wise evaluation of a 4D CEST image with an ONNX network.
   *
   * The network takes one row per voxel with one column per volume (all
   * volumes after CESTOnnxConfig::ColShift, in the volume order of the image)
   * and returns one row per voxel with one column per parameter map. It does
   * the whole evaluation (normalization, B0 correction, MTRasym, ...) itself.
   *
   * Voxels for which the network returns a non-finite value (e.g. background
   * without signal) are set to 0 in the resulting map.
   */
  class MITKCESTONNX_EXPORT CESTOnnxEvaluator
  {
  public:
    /** One parameter map; the pixel type is float. */
    struct Map
    {
      std::string Name;
      Image::Pointer Image;
    };

    /**
     * Receives the progress in [0, 1]. Returning false cancels the evaluation.
     */
    using ProgressCallback = std::function<bool(double)>;

    /**
     * \throw mitk::Exception if the model cannot be loaded.
     */
    explicit CESTOnnxEvaluator(const std::string& modelPath);
    ~CESTOnnxEvaluator();

    CESTOnnxEvaluator(const CESTOnnxEvaluator&) = delete;
    CESTOnnxEvaluator& operator=(const CESTOnnxEvaluator&) = delete;

    /**
     * \brief Compute the maps defined in the config.
     *
     * \param image 4D image; each time step is one network input column.
     * \param config Maps to compute.
     * \param dicomRange If true, the values are converted to the DICOM range
     * with value * Scale + Bias and clipped to [0, 4095]; otherwise the raw network values are returned.
     * \param progress Optional progress callback.
     * \return The maps in the order of the config, or an empty vector if cancelled.
     * \throw mitk::Exception if the image does not match the model or config.
     */
    std::vector<Map> Evaluate(const Image* image,
                              const CESTOnnxConfig& config,
                              bool dicomRange,
                              const ProgressCallback& progress = {}) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
  };
}

#endif
