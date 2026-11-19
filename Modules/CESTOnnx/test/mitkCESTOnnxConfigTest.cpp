/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include <mitkTestFixture.h>
#include <mitkTestingMacros.h>

#include <mitkCESTOnnxConfig.h>
#include <mitkException.h>

namespace
{
  const std::string ValidIni =
    "[NumContrasts]\n"
    "Number = 3\n"
    "\n"
    "[Contrasts]\n"
    "1 = 1_MTRasym_3.5ppm\n"
    "2 = 2_dB0_ppm\n"
    "3 = 3_unused\n"
    "\n"
    "[1_MTRasym_3.5ppm]\n"
    "# value = (DICOM - 1000) / 10000\n"
    "Compute = 1\n"
    "AmplitudeId = 0\n"
    "Scale = 10000\n"
    "bias = 1000\n"
    "\n"
    "[2_dB0_ppm]\n"
    "Compute = 1\n"
    "AmplitudeId = 2\n"
    "Scale = 2000\n"
    "bias = 2000\n"
    "\n"
    "[3_unused]\n"
    "Compute = 0\n"
    "AmplitudeId = 1\n"
    "Scale = 1\n"
    "bias = 0\n"
    "\n"
    "[Offsets]\n"
    "numTotalOffsets = 26\n"
    "colShift = 1\n"
    "\n"
    "[normalization] # 0-none/ONNX intern 1-M0normalisation\n"
    "normMethod = 0\n"
    "\n"
    "[B1_Mapping]\n"
    "enableB1cor = 0\n";
}

class mitkCESTOnnxConfigTestSuite : public mitk::TestFixture
{
  CPPUNIT_TEST_SUITE(mitkCESTOnnxConfigTestSuite);
  MITK_TEST(ValidIni_Success);
  MITK_TEST(NormMethodNotZero_Failure);
  MITK_TEST(B1Enabled_Failure);
  MITK_TEST(MissingContrastSection_Failure);
  MITK_TEST(NoContrasts_Failure);
  CPPUNIT_TEST_SUITE_END();

public:
  void ValidIni_Success()
  {
    const auto config = mitk::CESTOnnxConfig::Parse(ValidIni);

    CPPUNIT_ASSERT_EQUAL(std::size_t(2), config.Contrasts.size());
    CPPUNIT_ASSERT_EQUAL(std::string("1_MTRasym_3.5ppm"), config.Contrasts[0].Name);
    CPPUNIT_ASSERT_EQUAL(std::size_t(0), config.Contrasts[0].AmplitudeId);
    CPPUNIT_ASSERT_EQUAL(10000.0, config.Contrasts[0].Scale);
    CPPUNIT_ASSERT_EQUAL(1000.0, config.Contrasts[0].Bias);
    CPPUNIT_ASSERT_EQUAL(std::string("2_dB0_ppm"), config.Contrasts[1].Name);
    CPPUNIT_ASSERT_EQUAL(std::size_t(2), config.Contrasts[1].AmplitudeId);
    CPPUNIT_ASSERT_EQUAL(std::size_t(1), config.ColShift);
    CPPUNIT_ASSERT_EQUAL(std::size_t(26), config.NumTotalOffsets);
  }

  void NormMethodNotZero_Failure()
  {
    std::string ini = ValidIni;
    ini.replace(ini.find("normMethod = 0"), 14, "normMethod = 1");
    CPPUNIT_ASSERT_THROW(mitk::CESTOnnxConfig::Parse(ini), mitk::Exception);
  }

  void B1Enabled_Failure()
  {
    std::string ini = ValidIni;
    ini.replace(ini.find("enableB1cor = 0"), 15, "enableB1cor = 1");
    CPPUNIT_ASSERT_THROW(mitk::CESTOnnxConfig::Parse(ini), mitk::Exception);
  }

  void MissingContrastSection_Failure()
  {
    std::string ini = ValidIni;
    ini.replace(ini.find("[2_dB0_ppm]"), 11, "[renamed]");
    CPPUNIT_ASSERT_THROW(mitk::CESTOnnxConfig::Parse(ini), mitk::Exception);
  }

  void NoContrasts_Failure()
  {
    CPPUNIT_ASSERT_THROW(mitk::CESTOnnxConfig::Parse("[Offsets]\nnumTotalOffsets = 3\n"), mitk::Exception);
  }
};

MITK_TEST_SUITE_REGISTRATION(mitkCESTOnnxConfig)
