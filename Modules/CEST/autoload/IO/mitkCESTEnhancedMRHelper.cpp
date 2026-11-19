/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include "mitkCESTEnhancedMRHelper.h"

#include <mitkDICOMDCMTKTagScanner.h>
#include <mitkDICOMProperty.h>
#include <mitkDICOMTagPath.h>
#include <mitkException.h>

#include <regex>

namespace
{
  const std::string ENHANCED_MR_IMAGE_STORAGE = "1.2.840.10008.5.1.4.1.1.4.1";

  std::string FindValue(const mitk::DICOMDatasetAccess& dataset, const mitk::DICOMTagPath& path)
  {
    for (const auto& finding : dataset.GetTagValueAsString(path))
    {
      if (finding.isValid)
      {
        return finding.value;
      }
    }
    return {};
  }

  std::optional<double> ParseNumberAfter(const std::string& comment, const std::regex& pattern)
  {
    std::smatch match;
    if (!std::regex_search(comment, match, pattern))
    {
      return std::nullopt;
    }

    try
    {
      return mitk::ConvertDICOMStrToValue<double>(match[1].str());
    }
    catch (const mitk::Exception&)
    {
      return std::nullopt;
    }
  }
}

namespace mitk
{
  CESTEnhancedMRFileInfo ReadCESTEnhancedMRFileInfo(const std::string& file)
  {
    const DICOMTagPath sopClassPath(0x0008, 0x0016);
    const DICOMTagPath seriesUIDPath(0x0020, 0x000E);
    const DICOMTagPath seriesDescriptionPath(0x0008, 0x103E);
    const DICOMTagPath instanceNumberPath(0x0020, 0x0013);
    const DICOMTagPath commentPath(0x0020, 0x4000);

    auto scanner = DICOMDCMTKTagScanner::New();
    scanner->AddTagPaths({ sopClassPath, seriesUIDPath, seriesDescriptionPath, instanceNumberPath, commentPath });
    scanner->SetInputFiles({ file });
    scanner->Scan();

    CESTEnhancedMRFileInfo info;

    const auto frames = scanner->GetFrameInfoList();
    if (frames.empty())
    {
      return info;
    }

    const DICOMDatasetAccess& dataset = *frames.front();

    info.isEnhancedMR = FindValue(dataset, sopClassPath) == ENHANCED_MR_IMAGE_STORAGE;
    info.seriesInstanceUID = FindValue(dataset, seriesUIDPath);
    info.seriesDescription = FindValue(dataset, seriesDescriptionPath);
    info.imageComment = FindValue(dataset, commentPath);

    const std::string instanceNumber = FindValue(dataset, instanceNumberPath);
    if (!instanceNumber.empty())
    {
      try
      {
        info.instanceNumber = ConvertDICOMStrToValue<int>(instanceNumber);
      }
      catch (const mitk::Exception&)
      {
        info.instanceNumber = std::nullopt;
      }
    }

    return info;
  }

  std::optional<double> ParseCESTOffsetPpm(const std::string& comment)
  {
    static const std::regex pattern(R"(Offset:\s*(-?\d+(?:\.\d+)?)\s*ppm)");
    return ParseNumberAfter(comment, pattern);
  }

  std::optional<double> ParseCESTB1Microtesla(const std::string& comment)
  {
    static const std::regex pattern(R"(B1 Power:\s*(-?\d+(?:\.\d+)?)\s*uT)");
    return ParseNumberAfter(comment, pattern);
  }

  std::optional<double> ReadTransmitFrequencyMHz(const std::string& file)
  {
    DICOMTagPath path;
    path.AddAnySelection(0x5200, 0x9229);
    path.AddAnySelection(0x0018, 0x9006);
    path.AddElement(0x0018, 0x9098);

    auto scanner = DICOMDCMTKTagScanner::New();
    scanner->AddTagPath(path);
    scanner->SetInputFiles({ file });
    scanner->Scan();

    const auto frames = scanner->GetFrameInfoList();
    if (frames.empty())
    {
      return std::nullopt;
    }

    const std::string value = FindValue(*frames.front(), path);
    if (value.empty())
    {
      return std::nullopt;
    }

    try
    {
      return ConvertDICOMStrToValue<double>(value);
    }
    catch (const mitk::Exception&)
    {
      return std::nullopt;
    }
  }
}
