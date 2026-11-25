/*
 * DrpAiRuntimeWrapper
 *
 *
 * (C) Copyright Renesas Electronics Corporation
 */

#ifndef DRPAI_RUNTIME_API_H_
#define DRPAI_RUNTIME_API_H_

#include <string>
#include <memory>
#include <vector>
#include <tuple>
#include <cstdint>

enum class InOutDataType {
  FLOAT32,
  FLOAT16,
  INT32,
  INT64,
  OTHER
};

class DrpAiRuntimeWrapper {
public:
  explicit DrpAiRuntimeWrapper(int device_type);
  DrpAiRuntimeWrapper();
  ~DrpAiRuntimeWrapper();

  /* Deploy the DRP-AI Translator i8 model into DRP-AI memory. */
  bool LoadModel(const std::string& model_dir, uint64_t start_address);
  uint64_t GetLastAddress();

  /* Copy data to the input layer. */
  void SetInput(int input_index, const float* data_ptr);
  void SetInput(int input_index, const uint16_t* data_ptr);

  int GetNumInput(std::string model_dir);
  InOutDataType GetInputDataType(int index);
  int GetNumOutput();
  std::tuple<InOutDataType, void*, int64_t> GetOutput(int index);

  /* Zero copy api. */
  std::vector<std::tuple<std::string, size_t, InOutDataType>> GetInputInfo();
  void* GetInputPtr(const std::string& name);
  std::vector<std::tuple<std::string, size_t, InOutDataType>> GetOutputInfo();
  void* GetOutputPtr(const std::string& name);

  /* Run AI model graph. */
  void Run(int freq_index);
  void Run();

  void ProfileRun(const std::string& profile_table, const std::string& profile_csv);
  void ProfileRun(const std::string& profile_table, const std::string& profile_csv, int freq_index);

  class Impl;

private:
  std::unique_ptr<Impl> impl_;
  int device_type_;
};

#endif /* DRPAI_RUNTIME_API_H_ */