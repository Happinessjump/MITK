/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include "mitkCESTNiftiReaderService.h"

#include "mitkCESTSidecarHelper.h"

#include <mitkCESTImageNormalizationFilter.h>
#include <mitkCESTPropertyHelper.h>
#include <mitkCustomMimeType.h>
#include <mitkException.h>
#include <mitkFileReaderSelector.h>
#include <mitkIOMimeTypes.h>
#include <mitkImage.h>
#include <mitkLog.h>
#include <mitkStringProperty.h>

#include <itkNiftiImageIO.h>

#include <sstream>

namespace
{
  const std::string DESCRIPTION = "NIfTI CEST Import";
  const std::string OPTION_NAME_NORMALIZE = "Normalize data";
  const std::string OPTION_VALUE_AUTOMATIC = "Automatic";
  const std::string OPTION_VALUE_NO = "No";

  /** Number of time steps stored in the header, or 0 if the file is not a readable 4D image. */
  std::size_t ReadTimeStepCount(const std::string& file)
  {
    auto imageIO = itk::NiftiImageIO::New();
    imageIO->SetFileName(file);

    try
    {
      imageIO->ReadImageInformation();
    }
    catch (const itk::ExceptionObject&)
    {
      return 0;
    }

    return imageIO->GetNumberOfDimensions() >= 4 ? imageIO->GetDimensions(3) : 0;
  }

  /** Set while the regular reader is looked up, so that this reader reports itself as unsupported. */
  thread_local bool delegating = false;

  struct DelegationGuard
  {
    DelegationGuard() { delegating = true; }
    ~DelegationGuard() { delegating = false; }
  };

  mitk::Image::Pointer ReadWithRegularReader(const std::string& file, std::size_t timeStepCount)
  {
    const DelegationGuard guard;

    mitk::FileReaderSelector selector(file);
    const auto items = selector.Get();

    // The best match is last.
    for (auto item = items.rbegin(); item != items.rend(); ++item)
    {
      if (item->GetDescription() == DESCRIPTION || item->GetConfidenceLevel() == mitk::IFileReader::Unsupported ||
          item->GetReader() == nullptr)
      {
        continue;
      }

      try
      {
        item->GetReader()->SetInput(file);

        for (const auto& data : item->GetReader()->Read())
        {
          mitk::Image::Pointer image = dynamic_cast<mitk::Image*>(data.GetPointer());
          if (image.IsNotNull() && image->GetTimeSteps() == timeStepCount)
          {
            return image;
          }
        }
      }
      catch (const std::exception& e)
      {
        MITK_WARN << "Reader \"" << item->GetDescription() << "\" failed for " << file << ": " << e.what();
      }
    }

    mitkThrow() << "Cannot load CEST file. No NIfTI reader provides a " << timeStepCount << "D time series for " << file;
  }

  std::string ToString(const std::vector<double>& values)
  {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());

    for (const double value : values)
    {
      stream << value << " ";
    }
    return stream.str();
  }
}

namespace mitk
{
  CESTNiftiReaderService::CESTNiftiReaderService()
    : AbstractFileReader(CustomMimeType(IOMimeTypes::NIFTI_MIMETYPE_NAME()), DESCRIPTION)
  {
    Options options;
    options[OPTION_NAME_NORMALIZE] = std::vector<std::string>{ OPTION_VALUE_AUTOMATIC, OPTION_VALUE_NO };
    options[CESTProtocolFileOptionName()] = std::string();
    options[CESTProtocolFileOptionName() + ".filter"] = CESTProtocolFileOptionFilter();
    this->SetDefaultOptions(options);

    // Must outrank the regular NIfTI reader, which is registered for the same MIME type.
    this->SetRanking(20);
    this->RegisterService();
  }

  IFileReader::ConfidenceLevel CESTNiftiReaderService::GetConfidenceLevel() const
  {
    if (delegating || AbstractFileReader::GetConfidenceLevel() != Supported)
    {
      return Unsupported;
    }

    const std::string file = this->GetInputLocation();
    const std::size_t timeSteps = ReadTimeStepCount(file);

    if (timeSteps <= 1)
    {
      return Unsupported;
    }

    // Without protocol file the regular reader stays the default, this reader can still be chosen.
    return ReadCESTSidecar(file, timeSteps).has_value() ? Supported : PartiallySupported;
  }

  std::vector<itk::SmartPointer<BaseData>> CESTNiftiReaderService::DoRead()
  {
    const std::string file = this->GetInputLocation();

    const std::size_t timeSteps = ReadTimeStepCount(file);
    const Options options = this->GetOptions();
    const auto protocolFileOption = options.find(CESTProtocolFileOptionName());
    const std::string protocolFile = protocolFileOption != options.end() ? protocolFileOption->second.ToString() : std::string();

    const auto sidecar = ResolveCESTSidecar(file, timeSteps, protocolFile);

    Image::Pointer image = ReadWithRegularReader(file, timeSteps);

    if (sidecar.has_value())
    {
      MITK_INFO << "Using " << sidecar->offsetsPpm.size() << " CEST offsets from the protocol file";

      image->GetPropertyList()->SetStringProperty(CEST_PROPERTY_NAME_OFFSETS().c_str(), ToString(sidecar->offsetsPpm).c_str());
      ApplyCESTSidecarParameters(image, sidecar.value());
    }
    else
    {
      image->GetPropertyList()->SetStringProperty(CEST_PROPERTY_NAME_OFFSETS().c_str(), ToString(SequentialCESTOffsets(timeSteps)).c_str());
    }

    std::string name;
    image->GetPropertyList()->GetStringProperty("name", name);

    std::vector<itk::SmartPointer<BaseData>> result;

    const std::string normalization = options.find(OPTION_NAME_NORMALIZE)->second.ToString();

    if (normalization != OPTION_VALUE_NO && IsNotNormalizedCESTImage(image))
    {
      MITK_INFO << "Unnormalized CEST image was loaded and will be normalized automatically.";

      auto normalizationFilter = CESTImageNormalizationFilter::New();
      normalizationFilter->SetInput(image);
      normalizationFilter->Update();

      Image::Pointer normalizedImage = normalizationFilter->GetOutput();
      if (!name.empty())
      {
        normalizedImage->SetProperty("name", StringProperty::New(name + "_normalized"));
      }
      result.push_back(normalizedImage.GetPointer());
    }
    else
    {
      result.push_back(image.GetPointer());
    }

    return result;
  }

  CESTNiftiReaderService* CESTNiftiReaderService::Clone() const
  {
    return new CESTNiftiReaderService(*this);
  }
}
