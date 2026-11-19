/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include "mitkCESTEnhancedMRReaderService.h"

#include "mitkCESTEnhancedMRHelper.h"
#include "mitkCESTSidecarHelper.h"

#include <mitkCESTImageNormalizationFilter.h>
#include <mitkCESTPropertyHelper.h>
#include <mitkCustomTagParser.h>
#include <mitkException.h>
#include <mitkITKImageImport.h>
#include <mitkImage.h>
#include <mitkLog.h>
#include <mitkStringProperty.h>

#include <itkGDCMImageIO.h>
#include <itkImage.h>
#include <itkImageFileReader.h>

#include <itksys/Directory.hxx>
#include <itksys/SystemTools.hxx>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
  const std::string OPTION_NAME_NORMALIZE = "Normalize data";
  const std::string OPTION_VALUE_AUTOMATIC = "Automatic";
  const std::string OPTION_VALUE_NO = "No";

  using VolumeType = itk::Image<float, 3>;

  struct SeriesFile
  {
    std::string path;
    mitk::CESTEnhancedMRFileInfo info;
  };

  bool IsDICOMCandidate(const std::string& name)
  {
    const std::string extension = itksys::SystemTools::LowerCase(itksys::SystemTools::GetFilenameLastExtension(name));
    return extension.empty() || extension == ".dcm";
  }

  /** Collect all Enhanced MR files of the series of the input file, ordered by instance number. */
  std::vector<SeriesFile> CollectSeriesFiles(const std::string& inputFile, const mitk::CESTEnhancedMRFileInfo& inputInfo)
  {
    const std::string directory = itksys::SystemTools::GetFilenamePath(inputFile);

    itksys::Directory listing;
    if (!listing.Load(directory))
    {
      mitkThrow() << "Cannot load CEST series. Cannot read directory: " << directory;
    }

    std::vector<SeriesFile> result;

    for (unsigned long i = 0; i < listing.GetNumberOfFiles(); ++i)
    {
      const std::string name = listing.GetFile(i);
      const std::string path = directory + "/" + name;

      if (name == "." || name == ".." || itksys::SystemTools::FileIsDirectory(path) || !IsDICOMCandidate(name))
      {
        continue;
      }

      const auto info = itksys::SystemTools::SameFile(path, inputFile) ? inputInfo : mitk::ReadCESTEnhancedMRFileInfo(path);

      if (info.isEnhancedMR && info.seriesInstanceUID == inputInfo.seriesInstanceUID)
      {
        if (!info.instanceNumber.has_value())
        {
          mitkThrow() << "Cannot load CEST series. File has no instance number: " << path;
        }

        result.push_back({ path, info });
      }
    }

    std::sort(result.begin(), result.end(), [](const SeriesFile& a, const SeriesFile& b) {
      return a.info.instanceNumber.value() < b.info.instanceNumber.value();
    });

    return result;
  }

  std::vector<double> ParseNumberList(const std::string& text)
  {
    std::istringstream stream(text);
    stream.imbue(std::locale("C"));

    std::vector<double> result;
    double value = 0.;
    while (stream >> value)
    {
      result.push_back(value);
    }
    return result;
  }

  /** Offsets in ppm in the order of the files. */
  std::vector<double> DetermineOffsets(const std::vector<SeriesFile>& files, const std::string& directory, const std::optional<mitk::CESTSidecarInfo>& sidecar)
  {
    std::vector<double> offsets;
    for (const auto& file : files)
    {
      const auto offset = mitk::ParseCESTOffsetPpm(file.info.imageComment);
      if (!offset.has_value())
      {
        offsets.clear();
        break;
      }
      offsets.push_back(offset.value());
    }

    if (offsets.empty())
    {
      offsets = ParseNumberList(mitk::CustomTagParser::ReadListFromFile(directory + "/LIST.txt"));
    }

    if (offsets.empty() && sidecar.has_value())
    {
      offsets = sidecar->offsetsPpm;
    }

    if (offsets.size() != files.size())
    {
      mitkThrow() << "Cannot load CEST series. Found " << files.size() << " files but " << offsets.size()
                  << " offsets. The offsets are expected in the image comments, in a LIST.txt or in a CEST protocol file (.ini, .log).";
    }

    return offsets;
  }

  VolumeType::Pointer ReadVolume(const std::string& file)
  {
    auto imageIO = itk::GDCMImageIO::New();
    auto reader = itk::ImageFileReader<VolumeType>::New();
    reader->SetImageIO(imageIO);
    reader->SetFileName(file);

    try
    {
      reader->Update();
    }
    catch (const itk::ExceptionObject& e)
    {
      mitkThrow() << "Cannot load CEST series. Cannot read file " << file << ": " << e.GetDescription();
    }

    return reader->GetOutput();
  }

  bool HaveSameGrid(const VolumeType& a, const VolumeType& b)
  {
    constexpr double tolerance = 1e-3;

    if (a.GetLargestPossibleRegion().GetSize() != b.GetLargestPossibleRegion().GetSize())
    {
      return false;
    }

    for (unsigned int d = 0; d < 3; ++d)
    {
      if (std::abs(a.GetSpacing()[d] - b.GetSpacing()[d]) > tolerance ||
          std::abs(a.GetOrigin()[d] - b.GetOrigin()[d]) > tolerance)
      {
        return false;
      }

      for (unsigned int e = 0; e < 3; ++e)
      {
        if (std::abs(a.GetDirection()[d][e] - b.GetDirection()[d][e]) > tolerance)
        {
          return false;
        }
      }
    }

    return true;
  }

  std::string ToString(const std::vector<double>& values)
  {
    std::ostringstream stream;
    stream.imbue(std::locale("C"));

    for (const double value : values)
    {
      stream << value << " ";
    }
    return stream.str();
  }
}

namespace mitk
{
  CESTEnhancedMRReaderService::CESTEnhancedMRReaderService(const CustomMimeType& mimeType, const std::string& description)
    : AbstractFileReader(mimeType, description)
  {
    Options options;
    options[OPTION_NAME_NORMALIZE] = std::vector<std::string>{ OPTION_VALUE_AUTOMATIC, OPTION_VALUE_NO };
    this->SetDefaultOptions(options);

    this->RegisterService();
  }

  std::vector<itk::SmartPointer<BaseData>> CESTEnhancedMRReaderService::DoRead()
  {
    std::vector<itk::SmartPointer<BaseData>> result;

    const std::string inputFile = this->GetInputLocation();

    const auto inputInfo = ReadCESTEnhancedMRFileInfo(inputFile);
    if (!inputInfo.isEnhancedMR)
    {
      mitkThrow() << "Cannot load CEST series. The file is not an Enhanced MR DICOM file: " << inputFile;
    }

    const auto files = CollectSeriesFiles(inputFile, inputInfo);
    const auto sidecar = ReadCESTSidecar(inputFile, files.size());
    const auto offsets = DetermineOffsets(files, itksys::SystemTools::GetFilenamePath(inputFile), sidecar);

    Image::Pointer image = Image::New();
    VolumeType::Pointer referenceVolume;

    for (std::size_t timeStep = 0; timeStep < files.size(); ++timeStep)
    {
      const auto volume = ReadVolume(files[timeStep].path);

      if (referenceVolume.IsNull())
      {
        referenceVolume = volume;
        const auto firstVolume = ImportItkImage(volume);
        image->Initialize(MakeScalarPixelType<float>(), *firstVolume->GetGeometry(), 1, static_cast<int>(files.size()));
      }
      else if (!HaveSameGrid(*volume, *referenceVolume))
      {
        mitkThrow() << "Cannot load CEST series. The image grid differs between the files of the series: " << files[timeStep].path;
      }

      image->SetVolume(volume->GetBufferPointer(), static_cast<int>(timeStep));
    }

    const std::string name = inputInfo.seriesDescription.empty()
      ? itksys::SystemTools::GetFilenameName(itksys::SystemTools::GetFilenamePath(inputFile))
      : inputInfo.seriesDescription;
    image->SetProperty("name", StringProperty::New(name));

    image->GetPropertyList()->SetStringProperty(CEST_PROPERTY_NAME_OFFSETS().c_str(), ToString(offsets).c_str());

    if (const auto b1 = ParseCESTB1Microtesla(files.front().info.imageComment); b1.has_value())
    {
      image->GetPropertyList()->SetStringProperty(CEST_PROPERTY_NAME_B1Amplitude().c_str(), ToString({ b1.value() }).c_str());
    }

    if (const auto frequency = ReadTransmitFrequencyMHz(files.front().path); frequency.has_value())
    {
      SetCESTFrequencyMHz(image, frequency.value());
    }

    if (sidecar.has_value())
    {
      ApplyCESTSidecarParameters(image, sidecar.value());
    }

    if (image->GetConstProperty(CEST_PROPERTY_NAME_FREQ().c_str()).IsNull())
    {
      MITK_WARN << "Neither the Transmit Frequency nor a CEST protocol file provides the frequency. The CEST frequency property is not set.";
    }

    const Options options = this->GetOptions();
    const std::string normalization = options.find(OPTION_NAME_NORMALIZE)->second.ToString();

    if (normalization != OPTION_VALUE_NO && IsNotNormalizedCESTImage(image))
    {
      MITK_INFO << "Unnormalized CEST image was loaded and will be normalized automatically.";

      auto normalizationFilter = CESTImageNormalizationFilter::New();
      normalizationFilter->SetInput(image);
      normalizationFilter->Update();

      Image::Pointer normalizedImage = normalizationFilter->GetOutput();
      normalizedImage->SetProperty("name", StringProperty::New(name + "_normalized"));
      result.push_back(normalizedImage.GetPointer());
    }
    else
    {
      result.push_back(image.GetPointer());
    }

    return result;
  }

  CESTEnhancedMRReaderService* CESTEnhancedMRReaderService::Clone() const
  {
    return new CESTEnhancedMRReaderService(*this);
  }
}
