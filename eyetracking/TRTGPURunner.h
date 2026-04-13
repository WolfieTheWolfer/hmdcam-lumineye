#pragma once
// TRTGPURunner.h
// Drop-in replacement for CuDLAStandaloneRunner using TensorRT on GPU.
// Exposes the same interface so EyeTrackingService needs minimal changes.

#include <boost/noncopyable.hpp>
#include <vector>
#include <cassert>
#include <stdexcept>
#include <cstring>
#include <cstdio>

#include <cuda_runtime.h>
#include <NvInfer.h>
#include "cudla.h"

#define TRT_CUDA_CHECK(call) \
  do { \
    cudaError_t status = (call); \
    if (status != cudaSuccess) { \
      fprintf(stderr, "CUDA error %s at %s:%d\n", cudaGetErrorString(status), __FILE__, __LINE__); \
      throw std::runtime_error("CUDA error"); \
    } \
  } while(0)

class TRTGPULogger : public nvinfer1::ILogger {
public:
  void log(Severity severity, const char* msg) noexcept override {
    if (severity <= Severity::kWARNING)
      fprintf(stderr, "[TRT] %s\n", msg);
  }
};

class TRTGPURunner : boost::noncopyable {
public:
  TRTGPURunner(const char* engineFile) {
    FILE* f = fopen(engineFile, "rb");
    if (!f) throw std::runtime_error(std::string("Cannot open engine: ") + engineFile);
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);
    initWithEngineData(data.data(), size);
  }

  TRTGPURunner(const uint8_t* engineData, size_t engineLen) {
    initWithEngineData(engineData, engineLen);
  }

  ~TRTGPURunner() {
    if (m_stream)      cudaStreamDestroy(m_stream);
    if (m_inputDev)    cudaFree(m_inputDev);
    if (m_outputDev)   cudaFree(m_outputDev);
    if (m_inputHost)   cudaFreeHost(m_inputHost);
    if (m_outputHost)  cudaFreeHost(m_outputHost);
    delete m_context;
    delete m_engine;
    delete m_runtime;
  }

  void runInference() {
    asyncStartInference();
    asyncFinishInference();
  }

  void asyncStartInference() {
    TRT_CUDA_CHECK(cudaMemcpyAsync(m_inputDev, m_inputHost, m_inputSize,
                                   cudaMemcpyHostToDevice, m_stream));

    // TRT 10 API -- set tensor addresses by name
    m_context->setTensorAddress(m_inputName.c_str(),  m_inputDev);
    m_context->setTensorAddress(m_outputName.c_str(), m_outputDev);
    if (!m_context->enqueueV3(m_stream))
        throw std::runtime_error("TRT enqueueV3 failed");

    TRT_CUDA_CHECK(cudaMemcpyAsync(m_outputHost, m_outputDev, m_outputSize,
                                   cudaMemcpyDeviceToHost, m_stream));
}

  void asyncFinishInference() {
    TRT_CUDA_CHECK(cudaStreamSynchronize(m_stream));
  }

  size_t inputTensorCount()  const { return 1; }
  size_t outputTensorCount() const { return 1; }

  const cudlaModuleTensorDescriptor& inputTensorDescriptor(size_t idx)  const { assert(idx==0); return m_inputDesc;  }
  const cudlaModuleTensorDescriptor& outputTensorDescriptor(size_t idx) const { assert(idx==0); return m_outputDesc; }

  template <typename T = void> T* inputTensorPtr(size_t idx)  const { assert(idx==0); return reinterpret_cast<T*>(m_inputHost);  }
  template <typename T = void> T* outputTensorPtr(size_t idx) const { assert(idx==0); return reinterpret_cast<T*>(m_outputHost); }

private:
  void fillDesc(cudlaModuleTensorDescriptor& desc, const nvinfer1::Dims& dims, size_t sizeBytes) {
    memset(&desc, 0, sizeof(desc));
    desc.n    = (dims.nbDims > 0) ? dims.d[0] : 1;
    desc.c    = (dims.nbDims > 1) ? dims.d[1] : 1;
    desc.h    = (dims.nbDims > 2) ? dims.d[2] : 1;
    desc.w    = (dims.nbDims > 3) ? dims.d[3] : 1;
    desc.size = sizeBytes;
    desc.dataType     = CUDLA_DATA_TYPE_HALF;
    desc.stride[0]    = sizeof(_Float16);
    desc.stride[1]    = desc.w * sizeof(_Float16);
    desc.stride[2]    = desc.w * desc.h * sizeof(_Float16);
    desc.stride[3]    = desc.w * desc.h * desc.c * sizeof(_Float16);
  }

  void initWithEngineData(const uint8_t* data, size_t len) {
    m_runtime = nvinfer1::createInferRuntime(m_logger);
    if (!m_runtime) throw std::runtime_error("Failed to create TRT runtime");

    m_engine = m_runtime->deserializeCudaEngine(data, len);
    if (!m_engine) throw std::runtime_error("Failed to deserialize TRT engine");

    m_context = m_engine->createExecutionContext();
    if (!m_context) throw std::runtime_error("Failed to create TRT context");

    TRT_CUDA_CHECK(cudaStreamCreate(&m_stream));

    // TRT 10: use getTensorName + getTensorShape instead of getBindingDimensions
    m_inputName  = m_engine->getIOTensorName(0);
    m_outputName = m_engine->getIOTensorName(1);

    auto inputDims  = m_engine->getTensorShape(m_inputName.c_str());
    auto outputDims = m_engine->getTensorShape(m_outputName.c_str());

    // Compute buffer sizes from dimensions
    m_inputSize = sizeof(_Float16);
    for (int i = 0; i < inputDims.nbDims; ++i)
      m_inputSize *= inputDims.d[i];

    m_outputSize = sizeof(_Float16);
    for (int i = 0; i < outputDims.nbDims; ++i)
      m_outputSize *= outputDims.d[i];

    TRT_CUDA_CHECK(cudaMalloc(&m_inputDev,   m_inputSize));
    TRT_CUDA_CHECK(cudaMalloc(&m_outputDev,  m_outputSize));
    TRT_CUDA_CHECK(cudaMallocHost(&m_inputHost,  m_inputSize));
    TRT_CUDA_CHECK(cudaMallocHost(&m_outputHost, m_outputSize));

    fillDesc(m_inputDesc,  inputDims,  m_inputSize);
    fillDesc(m_outputDesc, outputDims, m_outputSize);

    printf("[TRTGPURunner] Input  [%lu,%lu,%lu,%lu] size=%zu\n",
      m_inputDesc.n, m_inputDesc.c, m_inputDesc.h, m_inputDesc.w, m_inputSize);
    printf("[TRTGPURunner] Output [%lu,%lu,%lu,%lu] size=%zu\n",
      m_outputDesc.n, m_outputDesc.c, m_outputDesc.h, m_outputDesc.w, m_outputSize);
  }

  TRTGPULogger                  m_logger;
  nvinfer1::IRuntime*           m_runtime  = nullptr;
  nvinfer1::ICudaEngine*        m_engine   = nullptr;
  nvinfer1::IExecutionContext*  m_context  = nullptr;
  cudaStream_t                  m_stream   = nullptr;

  void*  m_inputDev    = nullptr;
  void*  m_outputDev   = nullptr;
  void*  m_inputHost   = nullptr;
  void*  m_outputHost  = nullptr;
  size_t m_inputSize   = 0;
  size_t m_outputSize  = 0;

  cudlaModuleTensorDescriptor m_inputDesc;
  cudlaModuleTensorDescriptor m_outputDesc;

  std::string m_inputName;
  std::string m_outputName;
};
