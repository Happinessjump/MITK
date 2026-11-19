/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#ifndef mitkCESTEnhancedMRReaderService_h
#define mitkCESTEnhancedMRReaderService_h

#include <mitkAbstractFileReader.h>

namespace mitk
{
  /**
   * \brief Reader for CEST series stored as Enhanced MR DICOM files.
   *
   * The DICOM series readers of MITK sort single-frame images by their top-level
   * position tags and cannot assemble Enhanced MR objects. This reader instead
   * treats every file of the selected series as one volume (one saturation
   * offset), orders the volumes by instance number and stacks them into one 4D image.
   *
   * The offsets are taken from the image comment of each file or, as fallback,
   * from a LIST.txt next to the files.
   */
  class CESTEnhancedMRReaderService : public AbstractFileReader
  {
  public:
    CESTEnhancedMRReaderService(const CustomMimeType& mimeType, const std::string& description);

  protected:
    CESTEnhancedMRReaderService(const CESTEnhancedMRReaderService&) = default;
    CESTEnhancedMRReaderService& operator=(const CESTEnhancedMRReaderService&) = delete;

    std::vector<itk::SmartPointer<BaseData>> DoRead() override;

  private:
    CESTEnhancedMRReaderService* Clone() const override;
  };
}

#endif
