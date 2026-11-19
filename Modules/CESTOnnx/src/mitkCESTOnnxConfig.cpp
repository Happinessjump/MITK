/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include "mitkCESTOnnxConfig.h"

#include <mitkExceptionMacro.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

namespace
{
  using Section = std::map<std::string, std::string>;
  using Ini = std::map<std::string, Section>;

  std::string ToLower(std::string s)
  {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
  }

  std::string Trim(const std::string& s)
  {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
      return {};
    return s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
  }

  std::string StripInlineComment(const std::string& s)
  {
    for (std::size_t i = 0; i < s.size(); ++i)
    {
      if ((s[i] == '#' || s[i] == ';') && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t'))
        return s.substr(0, i);
    }
    return s;
  }

  // Section names and keys are case insensitive; a comment may follow a section header.
  Ini ParseIni(const std::string& text)
  {
    Ini ini;
    Section* current = nullptr;
    std::istringstream stream(text);
    std::string rawLine;

    while (std::getline(stream, rawLine))
    {
      const std::string line = Trim(StripInlineComment(rawLine));
      if (line.empty())
        continue;

      if (line.front() == '[')
      {
        const auto close = line.find(']');
        if (close == std::string::npos)
          mitkThrow() << "Malformed section header in ini file: " << line;
        current = &ini[ToLower(Trim(line.substr(1, close - 1)))];
        continue;
      }

      const auto equals = line.find('=');
      if (current == nullptr || equals == std::string::npos)
        mitkThrow() << "Malformed line in ini file: " << line;
      (*current)[ToLower(Trim(line.substr(0, equals)))] = Trim(line.substr(equals + 1));
    }

    return ini;
  }

  const std::string& Require(const Ini& ini, const std::string& section, const std::string& key)
  {
    const auto sectionIter = ini.find(ToLower(section));
    if (sectionIter == ini.end())
      mitkThrow() << "Missing section [" << section << "] in ini file.";
    const auto keyIter = sectionIter->second.find(ToLower(key));
    if (keyIter == sectionIter->second.end())
      mitkThrow() << "Missing entry '" << key << "' in section [" << section << "] of ini file.";
    return keyIter->second;
  }

  double ToDouble(const std::string& value, const std::string& context)
  {
    try
    {
      std::size_t used = 0;
      const double result = std::stod(value, &used);
      if (used == value.size())
        return result;
    }
    catch (const std::exception&)
    {
    }
    mitkThrow() << "Entry '" << context << "' is not a number: " << value;
  }

  std::size_t ToIndex(const std::string& value, const std::string& context)
  {
    const double number = ToDouble(value, context);
    if (number < 0.0 || number != static_cast<double>(static_cast<std::size_t>(number)))
      mitkThrow() << "Entry '" << context << "' is not a non-negative integer: " << value;
    return static_cast<std::size_t>(number);
  }

  // Only for entries where an absent value means exactly the same as 0.
  std::size_t OptionalIndex(const Ini& ini, const std::string& section, const std::string& key)
  {
    const auto sectionIter = ini.find(ToLower(section));
    if (sectionIter == ini.end() || sectionIter->second.count(ToLower(key)) == 0)
      return 0;
    return ToIndex(sectionIter->second.at(ToLower(key)), section + "/" + key);
  }
}

mitk::CESTOnnxConfig mitk::CESTOnnxConfig::Load(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file)
    mitkThrow() << "Cannot open ini file: " << path;

  std::ostringstream content;
  content << file.rdbuf();
  return Parse(content.str());
}

mitk::CESTOnnxConfig mitk::CESTOnnxConfig::Parse(const std::string& text)
{
  const Ini ini = ParseIni(text);

  if (OptionalIndex(ini, "normalization", "normMethod") != 0)
    mitkThrow() << "Only normMethod = 0 (normalization inside the network) is supported.";
  if (OptionalIndex(ini, "B1_Mapping", "enableB1cor") != 0)
    mitkThrow() << "B1 correction is not supported.";

  CESTOnnxConfig config;
  config.ColShift = OptionalIndex(ini, "Offsets", "colShift");
  config.NumTotalOffsets = OptionalIndex(ini, "Offsets", "numTotalOffsets");

  // The numeric keys of [Contrasts] define the order of the maps.
  const auto contrasts = ini.find("contrasts");
  if (contrasts == ini.end())
    mitkThrow() << "Missing section [Contrasts] in ini file.";

  std::map<std::size_t, std::string> byNumber;
  for (const auto& [key, name] : contrasts->second)
    byNumber[ToIndex(key, "Contrasts/" + key)] = name;

  for (const auto& [number, name] : byNumber)
  {
    if (ini.count(ToLower(name)) == 0)
      mitkThrow() << "Section [" << name << "] listed in [Contrasts] is missing in ini file.";

    if (ToIndex(Require(ini, name, "Compute"), name + "/Compute") == 0)
      continue;

    CESTOnnxContrast contrast;
    contrast.Name = name;
    contrast.AmplitudeId = ToIndex(Require(ini, name, "AmplitudeId"), name + "/AmplitudeId");
    contrast.Scale = ToDouble(Require(ini, name, "Scale"), name + "/Scale");
    contrast.Bias = ToDouble(Require(ini, name, "bias"), name + "/bias");
    config.Contrasts.push_back(contrast);
  }

  if (config.Contrasts.empty())
    mitkThrow() << "The ini file does not define any map with Compute = 1.";

  return config;
}
