/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include "mitkCESTSidecarHelper.h"

#include <mitkCESTPropertyHelper.h>
#include <mitkImage.h>
#include <mitkLog.h>

#include <itksys/Directory.hxx>
#include <itksys/SystemTools.hxx>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <locale>
#include <map>
#include <sstream>

namespace
{
  /** Offsets with a larger magnitude are M0 scans and carry no meaningful B1 value. */
  constexpr double MAX_SATURATION_OFFSET_PPM = 299.;

  std::string Trim(const std::string& text)
  {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
    {
      return {};
    }
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
  }

  std::vector<std::string> SplitWhitespace(const std::string& text)
  {
    std::istringstream stream(text);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token)
    {
      tokens.push_back(token);
    }
    return tokens;
  }

  /** Locale independent, as the application locale may use a decimal comma. */
  std::optional<double> ToDouble(const std::string& text)
  {
    const std::string trimmed = Trim(text);
    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();

    if (begin != end && *begin == '+')
    {
      ++begin;
    }

    double value = 0.;
    const auto [position, error] = std::from_chars(begin, end, value);
    if (error != std::errc() || position == begin)
    {
      return std::nullopt;
    }
    return value;
  }

  std::optional<double> FirstNumber(const std::string& text)
  {
    const auto tokens = SplitWhitespace(text);
    return tokens.empty() ? std::nullopt : ToDouble(tokens.front());
  }

  bool IsKey(const std::string& key, const std::string& expected)
  {
    return itksys::SystemTools::LowerCase(Trim(key)) == itksys::SystemTools::LowerCase(expected);
  }

  /** B1 of the first saturation scan; warns if the offsets use different values. */
  std::optional<double> DetermineB1(const std::vector<double>& offsets, const std::vector<std::optional<double>>& b1Values)
  {
    std::optional<double> result;
    for (std::size_t i = 0; i < offsets.size(); ++i)
    {
      if (std::abs(offsets[i]) > MAX_SATURATION_OFFSET_PPM || !b1Values[i].has_value())
      {
        continue;
      }

      if (!result.has_value())
      {
        result = b1Values[i];
      }
      else if (std::abs(result.value() - b1Values[i].value()) > 1e-6)
      {
        MITK_WARN << "The CEST protocol file contains different B1 values. The first one (" << result.value()
                  << " uT) is used.";
        break;
      }
    }
    return result;
  }

  /** The log writes the Larmor frequency as "FREQ: 123.2 Hz", although the value is given in MHz. */
  double NormalizeFrequencyToMHz(double value)
  {
    return value < 1000. ? value : value * 1e-6;
  }

  std::optional<mitk::CESTSidecarInfo> ParseIni(std::istream& stream)
  {
    std::map<int, std::pair<double, std::optional<double>>> offsets;
    std::optional<double> pulseLengthUs;
    std::optional<double> interpulseDelayUs;

    std::string line;
    while (std::getline(stream, line))
    {
      line = Trim(line);
      if (line.empty() || line.front() == '#' || line.front() == ';' || line.front() == '[')
      {
        continue;
      }

      const auto separator = line.find('=');
      if (separator == std::string::npos)
      {
        continue;
      }

      const std::string key = Trim(line.substr(0, separator));
      const std::string value = line.substr(separator + 1);

      if (itksys::SystemTools::LowerCase(key).rfind("offset[", 0) == 0)
      {
        const auto close = key.find(']');
        const auto tokens = SplitWhitespace(value);
        const auto ppm = tokens.empty() ? std::nullopt : ToDouble(tokens[0]);
        const auto index = ToDouble(key.substr(7, close == std::string::npos ? 0 : close - 7));
        if (ppm.has_value() && index.has_value())
        {
          offsets[static_cast<int>(index.value())] = { ppm.value(), tokens.size() > 1 ? ToDouble(tokens[1]) : std::nullopt };
        }
      }
      else if (IsKey(key, "PulseLength"))
      {
        pulseLengthUs = FirstNumber(value);
      }
      else if (IsKey(key, "InterpulseDelay"))
      {
        interpulseDelayUs = FirstNumber(value);
      }
    }

    if (offsets.empty())
    {
      return std::nullopt;
    }

    mitk::CESTSidecarInfo info;
    std::vector<std::optional<double>> b1Values;
    for (const auto& [index, entry] : offsets)
    {
      info.offsetsPpm.push_back(entry.first);
      b1Values.push_back(entry.second);
    }

    info.b1Microtesla = DetermineB1(info.offsetsPpm, b1Values);

    if (pulseLengthUs.has_value())
    {
      info.pulseDurationMs = pulseLengthUs.value() * 1e-3;

      if (interpulseDelayUs.has_value() && pulseLengthUs.value() + interpulseDelayUs.value() > 0.)
      {
        info.dutyCyclePercent = 100. * pulseLengthUs.value() / (pulseLengthUs.value() + interpulseDelayUs.value());
      }
    }

    return info;
  }

  std::optional<mitk::CESTSidecarInfo> ParseLog(std::istream& stream)
  {
    mitk::CESTSidecarInfo info;

    std::string line;
    while (std::getline(stream, line))
    {
      const auto separator = line.find(':');
      if (separator == std::string::npos)
      {
        continue;
      }

      const std::string key = line.substr(0, separator);
      const std::string value = line.substr(separator + 1);

      if (IsKey(key, "o"))
      {
        if (const auto ppm = FirstNumber(value); ppm.has_value())
        {
          info.offsetsPpm.push_back(ppm.value());
        }
      }
      else if (IsKey(key, "SAT_B1"))
      {
        info.b1Microtesla = FirstNumber(value);
      }
      else if (IsKey(key, "T_PULSE"))
      {
        info.pulseDurationMs = FirstNumber(value);
      }
      else if (IsKey(key, "DC"))
      {
        info.dutyCyclePercent = FirstNumber(value);
      }
      else if (IsKey(key, "FREQ"))
      {
        if (const auto frequency = FirstNumber(value); frequency.has_value())
        {
          info.frequencyMHz = NormalizeFrequencyToMHz(frequency.value());
        }
      }
    }

    if (info.offsetsPpm.empty())
    {
      return std::nullopt;
    }
    return info;
  }

  struct Candidate
  {
    std::string fileName;
    bool matchesImageName;
    bool isIni;
    mitk::CESTSidecarInfo info;
  };

  std::string ImageStem(const std::string& imageFile)
  {
    std::string stem = itksys::SystemTools::GetFilenameWithoutLastExtension(imageFile);
    if (itksys::SystemTools::LowerCase(itksys::SystemTools::GetFilenameLastExtension(stem)) == ".nii")
    {
      stem = itksys::SystemTools::GetFilenameWithoutLastExtension(stem);
    }
    return stem;
  }

  template <typename T>
  void FillIfMissing(std::optional<T>& target, const std::optional<T>& source)
  {
    if (!target.has_value())
    {
      target = source;
    }
  }

  void SetStringProperty(mitk::Image* image, const std::string& name, double value)
  {
    if (image->GetConstProperty(name.c_str()).IsNotNull())
    {
      return;
    }

    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << value;
    image->GetPropertyList()->SetStringProperty(name.c_str(), stream.str().c_str());
  }
}

namespace mitk
{
  std::optional<CESTSidecarInfo> ReadCESTSidecar(const std::string& imageFile, std::size_t timeStepCount)
  {
    const std::string directory = itksys::SystemTools::GetFilenamePath(imageFile);
    const std::string stem = ImageStem(imageFile);

    itksys::Directory listing;
    if (!listing.Load(directory))
    {
      return std::nullopt;
    }

    std::vector<Candidate> candidates;

    for (unsigned long i = 0; i < listing.GetNumberOfFiles(); ++i)
    {
      const std::string name = listing.GetFile(i);
      const std::string extension = itksys::SystemTools::LowerCase(itksys::SystemTools::GetFilenameLastExtension(name));
      const bool isIni = extension == ".ini";

      if (!isIni && extension != ".log")
      {
        continue;
      }

      std::ifstream stream(directory + "/" + name);
      if (!stream)
      {
        continue;
      }

      auto info = isIni ? ParseIni(stream) : ParseLog(stream);

      if (info.has_value() && (timeStepCount == 0 || info->offsetsPpm.size() == timeStepCount))
      {
        candidates.push_back({ name, itksys::SystemTools::GetFilenameWithoutLastExtension(name) == stem, isIni, info.value() });
      }
    }

    if (candidates.empty())
    {
      return std::nullopt;
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
      if (a.matchesImageName != b.matchesImageName)
      {
        return a.matchesImageName;
      }
      if (a.isIni != b.isIni)
      {
        return a.isIni;
      }
      return a.fileName < b.fileName;
    });

    CESTSidecarInfo result = candidates.front().info;
    MITK_DEBUG << "Using CEST protocol file " << candidates.front().fileName;

    for (std::size_t i = 1; i < candidates.size(); ++i)
    {
      const auto& other = candidates[i].info;

      if (other.offsetsPpm != result.offsetsPpm)
      {
        MITK_WARN << "The offsets of the CEST protocol file " << candidates[i].fileName << " differ from "
                  << candidates.front().fileName << ". The latter is used.";
        continue;
      }

      FillIfMissing(result.b1Microtesla, other.b1Microtesla);
      FillIfMissing(result.pulseDurationMs, other.pulseDurationMs);
      FillIfMissing(result.dutyCyclePercent, other.dutyCyclePercent);
      FillIfMissing(result.frequencyMHz, other.frequencyMHz);
    }

    return result;
  }

  void ApplyCESTSidecarParameters(Image* image, const CESTSidecarInfo& info)
  {
    if (info.b1Microtesla.has_value())
    {
      SetStringProperty(image, CEST_PROPERTY_NAME_B1Amplitude(), info.b1Microtesla.value());
    }

    if (info.pulseDurationMs.has_value())
    {
      // The property is stored in microseconds.
      SetStringProperty(image, CEST_PROPERTY_NAME_PULSEDURATION(), info.pulseDurationMs.value() * 1e3);
    }

    if (info.dutyCyclePercent.has_value())
    {
      SetStringProperty(image, CEST_PROPERTY_NAME_DutyCycle(), info.dutyCyclePercent.value());
    }

    if (info.frequencyMHz.has_value() && image->GetConstProperty(CEST_PROPERTY_NAME_FREQ().c_str()).IsNull())
    {
      SetCESTFrequencyMHz(image, info.frequencyMHz.value());
    }
  }
}
