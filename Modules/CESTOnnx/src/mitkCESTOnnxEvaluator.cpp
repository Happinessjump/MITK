/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#include "mitkCESTOnnxEvaluator.h"

#include <mitkExceptionMacro.h>
#include <mitkImageReadAccessor.h>
#include <mitkImageWriteAccessor.h>

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace
{
  // Voxels per network call; bounds memory and sets the granularity of the progress report.
  constexpr std::size_t ChunkSize = 1u << 16;
  constexpr double MaxDicomValue = 4095.0;

  template <typename T>
  void CopyColumn(const void* source, std::size_t numVoxels, std::size_t column, std::size_t numColumns, float* rows)
  {
    const T* values = static_cast<const T*>(source);
    for (std::size_t voxel = 0; voxel < numVoxels; ++voxel)
      rows[voxel * numColumns + column] = static_cast<float>(values[voxel]);
  }

  void CopyColumn(const mitk::PixelType& type,
                  const void* source,
                  std::size_t numVoxels,
                  std::size_t column,
                  std::size_t numColumns,
                  float* rows)
  {
    switch (type.GetComponentType())
    {
      case itk::IOComponentEnum::FLOAT: return CopyColumn<float>(source, numVoxels, column, numColumns, rows);
      case itk::IOComponentEnum::DOUBLE: return CopyColumn<double>(source, numVoxels, column, numColumns, rows);
      case itk::IOComponentEnum::SHORT: return CopyColumn<short>(source, numVoxels, column, numColumns, rows);
      case itk::IOComponentEnum::USHORT: return CopyColumn<unsigned short>(source, numVoxels, column, numColumns, rows);
      case itk::IOComponentEnum::INT: return CopyColumn<int>(source, numVoxels, column, numColumns, rows);
      case itk::IOComponentEnum::UINT: return CopyColumn<unsigned int>(source, numVoxels, column, numColumns, rows);
      case itk::IOComponentEnum::CHAR: return CopyColumn<char>(source, numVoxels, column, numColumns, rows);
      case itk::IOComponentEnum::UCHAR: return CopyColumn<unsigned char>(source, numVoxels, column, numColumns, rows);
      default: mitkThrow() << "Unsupported pixel type: " << type.GetComponentTypeAsString();
    }
  }

  std::size_t CountTokens(const std::string& text)
  {
    std::istringstream stream(text);
    std::string token;
    std::size_t count = 0;
    while (stream >> token)
      ++count;
    return count;
  }
}

struct mitk::CESTOnnxEvaluator::Impl
{
  explicit Impl(const std::string& modelPath)
    : Env(ORT_LOGGING_LEVEL_WARNING, "MitkCESTOnnx"), Session(nullptr)
  {
    try
    {
      Ort::SessionOptions options;
      const auto path = std::filesystem::u8path(modelPath);
      this->Session = Ort::Session(this->Env, path.c_str(), options);

      Ort::AllocatorWithDefaultOptions allocator;
      this->InputName = this->Session.GetInputNameAllocated(0, allocator).get();
      this->OutputName = this->Session.GetOutputNameAllocated(0, allocator).get();

      // Optional model metadata written by the network builder; used to catch protocol mismatches.
      auto metadata = this->Session.GetModelMetadata();
      auto offsets = metadata.LookupCustomMetadataMapAllocated("offsets_ppm_log_order", allocator);
      if (offsets)
        this->NumOffsets = CountTokens(offsets.get());
    }
    catch (const Ort::Exception& e)
    {
      mitkThrow() << "Cannot load ONNX model " << modelPath << ": " << e.what();
    }
  }

  Ort::Env Env;
  Ort::Session Session;
  std::string InputName;
  std::string OutputName;
  std::size_t NumOffsets = 0;
};

mitk::CESTOnnxEvaluator::CESTOnnxEvaluator(const std::string& modelPath)
  : m_Impl(std::make_unique<Impl>(modelPath))
{
}

mitk::CESTOnnxEvaluator::~CESTOnnxEvaluator() = default;

std::vector<mitk::CESTOnnxEvaluator::Map> mitk::CESTOnnxEvaluator::Evaluate(const Image* image,
                                                                            const CESTOnnxConfig& config,
                                                                            bool dicomRange,
                                                                            const ProgressCallback& progress) const
{
  if (image == nullptr)
    mitkThrow() << "No input image.";
  if (image->GetDimension() != 4)
    mitkThrow() << "The input image must be 4D (one time step per CEST volume).";

  const std::size_t numVolumes = image->GetDimension(3);
  if (config.NumTotalOffsets != 0 && config.NumTotalOffsets != numVolumes)
    mitkThrow() << "The image has " << numVolumes << " volumes, the ini file expects " << config.NumTotalOffsets << ".";
  if (config.ColShift >= numVolumes)
    mitkThrow() << "colShift of the ini file is not smaller than the number of volumes (" << numVolumes << ").";

  const std::size_t numColumns = numVolumes - config.ColShift;
  if (m_Impl->NumOffsets != 0 && m_Impl->NumOffsets != numColumns)
    mitkThrow() << "The model expects " << m_Impl->NumOffsets << " volumes, the image provides " << numColumns
                << " (after colShift).";

  const std::size_t numVoxels =
    static_cast<std::size_t>(image->GetDimension(0)) * image->GetDimension(1) * image->GetDimension(2);

  // One row per voxel, one column per volume.
  std::vector<float> rows(numVoxels * numColumns);
  for (std::size_t column = 0; column < numColumns; ++column)
  {
    ImageReadAccessor accessor(image, image->GetVolumeData(static_cast<int>(column + config.ColShift)));
    CopyColumn(image->GetPixelType(), accessor.GetData(), numVoxels, column, numColumns, rows.data());
  }

  std::vector<Map> maps;
  for (const auto& contrast : config.Contrasts)
  {
    Map map;
    map.Name = contrast.Name;
    map.Image = Image::New();
    map.Image->Initialize(MakeScalarPixelType<float>(), *image->GetGeometry(0), 1, 1);
    maps.push_back(map);
  }

  const char* inputNames[] = {m_Impl->InputName.c_str()};
  const char* outputNames[] = {m_Impl->OutputName.c_str()};
  const auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

  std::vector<std::unique_ptr<ImageWriteAccessor>> accessors;
  std::vector<float*> targets;
  for (auto& map : maps)
  {
    accessors.push_back(std::make_unique<ImageWriteAccessor>(map.Image, map.Image->GetVolumeData(0)));
    targets.push_back(static_cast<float*>(accessors.back()->GetData()));
  }

  try
  {
    for (std::size_t first = 0; first < numVoxels; first += ChunkSize)
    {
      if (progress && !progress(static_cast<double>(first) / static_cast<double>(numVoxels)))
        return {};

      const std::size_t count = std::min(ChunkSize, numVoxels - first);
      const std::array<int64_t, 2> shape = {static_cast<int64_t>(count), static_cast<int64_t>(numColumns)};
      auto input = Ort::Value::CreateTensor<float>(
        memoryInfo, rows.data() + first * numColumns, count * numColumns, shape.data(), shape.size());

      auto outputs = m_Impl->Session.Run(Ort::RunOptions{nullptr}, inputNames, &input, 1, outputNames, 1);

      const auto outputShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
      if (outputShape.size() != 2 || outputShape[0] != static_cast<int64_t>(count))
        mitkThrow() << "Unexpected output shape of the ONNX model; expected one row per voxel.";

      const std::size_t numOutputs = static_cast<std::size_t>(outputShape[1]);
      const float* output = outputs[0].GetTensorData<float>();

      for (std::size_t i = 0; i < config.Contrasts.size(); ++i)
      {
        const auto& contrast = config.Contrasts[i];
        if (contrast.AmplitudeId >= numOutputs)
          mitkThrow() << "Map " << contrast.Name << " uses output column " << contrast.AmplitudeId
                      << ", but the model only has " << numOutputs << " columns.";

        for (std::size_t voxel = 0; voxel < count; ++voxel)
        {
          double value = output[voxel * numOutputs + contrast.AmplitudeId];
          if (!std::isfinite(value))
            value = 0.0;
          else if (dicomRange)
            value = std::clamp(value * contrast.Scale + contrast.Bias, 0.0, MaxDicomValue);
          targets[i][first + voxel] = static_cast<float>(value);
        }
      }
    }
  }
  catch (const Ort::Exception& e)
  {
    mitkThrow() << "ONNX evaluation failed: " << e.what();
  }

  if (progress)
    progress(1.0);

  return maps;
}
