/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#ifndef mitkCESTNiftiReaderService_h
#define mitkCESTNiftiReaderService_h

#include <mitkAbstractFileReader.h>

namespace mitk
{
  /**
   * \brief Reads a 4D NIfTI image and attaches the CEST properties from a protocol file.
   *
   * The reader only applies if a CEST protocol file (see ReadCESTSidecar()) with as many
   * offsets as the image has time steps is located next to the image. The image data
   * itself is read by the regular NIfTI reader.
   */
  class CESTNiftiReaderService : public AbstractFileReader
  {
  public:
    CESTNiftiReaderService();

    using AbstractFileReader::Read;

    ConfidenceLevel GetConfidenceLevel() const override;

  protected:
    std::vector<itk::SmartPointer<BaseData>> DoRead() override;

  private:
    CESTNiftiReaderService* Clone() const override;
  };
}

#endif
