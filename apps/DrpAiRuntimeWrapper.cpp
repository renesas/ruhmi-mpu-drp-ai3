/*
 * DrpAiRuntimeWrapper implementation.
 *
 * Specification : DrpAiRuntimeWrapper_spec_v01.md
 * Target        : RZ/V2H
 *
 * (C) Copyright Renesas Electronics Corporation
 */

#include "DrpAiRuntimeWrapper.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <string>
#include <vector>
#include <tuple>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/drpai.h>

/*****************************************
 * Local constants
 *****************************************/
namespace {

constexpr const char* DRPAI_DEV      = "/dev/drpai0";
constexpr size_t      BUF_SIZE       = 1024 * 1024;   /* write/read chunk size */
constexpr int         DRPAI_TIMEOUT  = 5;             /* pselect timeout [sec]  */
constexpr uint32_t    DRP_MAX_FREQ   = 2;             /* mindiv (2 -> 420MHz)   */

/* logging helpers */
inline void log_err(const std::string& msg) {
  std::fprintf(stderr, "[ERROR][DrpAiRuntimeWrapper] %s (errno=%d)\n", msg.c_str(), errno);
}
inline void log_err_ne(const std::string& msg) {
  std::fprintf(stderr, "[ERROR][DrpAiRuntimeWrapper] %s\n", msg.c_str());
}
inline void log_warn(const std::string& msg) {
  std::fprintf(stderr, "[WARN ][DrpAiRuntimeWrapper] %s\n", msg.c_str());
}
inline void log_info(const std::string& msg) {
  std::fprintf(stdout, "[INFO ][DrpAiRuntimeWrapper] %s\n", msg.c_str());
}

/* trim whitespace */
std::string trim(const std::string& s) {
  const char* ws = " \t\r\n";
  const auto b = s.find_first_not_of(ws);
  if (b == std::string::npos) return "";
  const auto e = s.find_last_not_of(ws);
  return s.substr(b, e - b + 1);
}

/* map prepost yaml "type" string to InOutDataType */
InOutDataType type_from_string(const std::string& t) {
  std::string s = t;
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  if (s == "fp32" || s == "float32" || s == "float")  return InOutDataType::FLOAT32;
  if (s == "fp16" || s == "float16")                  return InOutDataType::FLOAT16;
  if (s == "int32")                                   return InOutDataType::INT32;
  if (s == "int64")                                   return InOutDataType::INT64;
  return InOutDataType::OTHER; /* uint8 / others */
}

/* element size [byte] for a given data type string (raw byte handling). */
size_t elem_size_from_string(const std::string& t) {
  std::string s = t;
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  if (s == "fp32" || s == "float32" || s == "float") return 4;
  if (s == "fp16" || s == "float16")                 return 2;
  if (s == "int32")                                  return 4;
  if (s == "int64")                                  return 8;
  if (s == "uint8" || s == "int8")                   return 1;
  if (s == "uint16" || s == "int16")                 return 2;
  return 1;
}

} /* anonymous namespace */

/*****************************************
 * Internal structures
 *****************************************/

/* Node (input/output) meta info combined from prepost yaml + data_*_list.txt */
struct NodeInfo {
  std::string          name;
  uint64_t             phys_addr = 0;   /* from data_*_list.txt */
  bool                 addr_valid = false;
  std::vector<int64_t> shape;
  std::string          order;
  InOutDataType        dtype = InOutDataType::OTHER;
  size_t               elem_size = 1;
  size_t               num_elem  = 0;
  size_t               bytes_size = 0;
};

/* Address/size of each Translator object file (relocated to start_address). */
struct ObjectAddress {
  uint64_t drp_config_addr = 0,      drp_config_size = 0;
  uint64_t drp_param_addr = 0,       drp_param_size = 0;
  uint64_t desc_aimac_addr = 0,      desc_aimac_size = 0;
  uint64_t desc_drp_addr = 0,        desc_drp_size = 0;
  uint64_t weight_addr = 0,          weight_size = 0;
  uint64_t aimac_cmd_addr = 0,       aimac_cmd_size = 0;
  uint64_t aimac_param_desc_addr = 0, aimac_param_desc_size = 0;
  uint64_t aimac_param_cmd_addr = 0,  aimac_param_cmd_size = 0;
  uint64_t data_in_addr = 0,         data_in_size = 0;
  uint64_t data_out_addr = 0,        data_out_size = 0;

  /* addr_map.txt origin (data_in relative base) and total object size. */
  uint64_t map_data_in_addr = 0;      /* org address of data_in in addr_map.txt */
  uint64_t object_files_size = 0;     /* total size of object region            */
  uint64_t last_address = 0;          /* end of placed objects                  */
};

/*****************************************
 * Impl
 *****************************************/
class DrpAiRuntimeWrapper::Impl {
public:
  explicit Impl(int device_type) : device_type_(device_type) {}
  ~Impl() { CloseDriver(); }

  bool LoadModel(const std::string& model_dir, uint64_t start_address);
  uint64_t GetLastAddress() const { return obj_.last_address; }

  template <typename T>
  void SetInputImpl(int input_index, const T* data_ptr);

  int GetNumInput(const std::string& model_dir);
  InOutDataType GetInputDataType(int index);
  int GetNumOutput() const { return static_cast<int>(outputs_.size()); }
  std::tuple<InOutDataType, void*, int64_t> GetOutput(int index);

  std::vector<std::tuple<std::string, size_t, InOutDataType>> GetInputInfo();
  void* GetInputPtr(const std::string& name);
  std::vector<std::tuple<std::string, size_t, InOutDataType>> GetOutputInfo();
  void* GetOutputPtr(const std::string& name);

  void Run(int freq_index);

private:
  /* --- parsing helpers --- */
  bool ParseAddrMap(const std::string& path, uint64_t start_address);
  bool ParsePrepostYaml(const std::string& path);
  bool ParseDataList(const std::string& path, bool is_input, uint64_t start_address);
  static std::string FindFileBySuffix(const std::string& dir, const std::string& suffix);
  static std::string FindFileByPrefixSuffix(const std::string& dir,
                                            const std::string& prefix,
                                            const std::string& suffix);

  /* --- driver object loading --- */
  bool LoadDrpaiData(const std::string& model_dir);
  bool LoadOneFile(const std::string& file, uint64_t addr, uint64_t size);
  bool LoadParamFile(const std::string& model_dir);

  /* --- run helpers --- */
  bool SetupAdrConv();
  void SetupProc(drpai_data_t* proc);

  void CloseDriver() {
    if (drpai_fd_ >= 0) {
      ::close(drpai_fd_);
      drpai_fd_ = -1;
    }
  }

  NodeInfo* FindNode(std::vector<NodeInfo>& v, const std::string& name) {
    for (auto& n : v) {
      if (n.name == name) return &n;
    }
    return nullptr;
  }

public:
  int          device_type_;

private:
  int          drpai_fd_ = -1;
  drpai_data_t drpai_area_{};
  uint64_t     start_address_ = 0;

  ObjectAddress obj_{};

  std::vector<NodeInfo> inputs_;
  std::vector<NodeInfo> outputs_;

  /* GetOutput host-side buffers (persist until next GetOutput of same index) */
  std::vector<std::vector<uint8_t>> output_host_buf_;

  /* Zero-copy GetInputPtr staging buffers (approach B, spec 4.11) */
  std::vector<std::vector<uint8_t>> input_staging_;

  uint32_t     drp_max_freq_ = DRP_MAX_FREQ;
  bool         loaded_ = false;
};

/*****************************************
 * File discovery helpers
 *****************************************/
std::string DrpAiRuntimeWrapper::Impl::FindFileBySuffix(const std::string& dir,
                                                        const std::string& suffix) {
  return FindFileByPrefixSuffix(dir, "", suffix);
}

std::string DrpAiRuntimeWrapper::Impl::FindFileByPrefixSuffix(const std::string& dir,
                                                              const std::string& prefix,
                                                              const std::string& suffix) {
  DIR* d = opendir(dir.c_str());
  if (!d) return "";
  std::string result;
  dirent* ent;
  while ((ent = readdir(d)) != nullptr) {
    std::string fn(ent->d_name);
    bool pre_ok = prefix.empty() || (fn.size() >= prefix.size() && fn.compare(0, prefix.size(), prefix) == 0);
    bool suf_ok = suffix.empty() || (fn.size() >= suffix.size() && fn.compare(fn.size() - suffix.size(), suffix.size(), suffix) == 0);
    if (pre_ok && suf_ok) {
      result = dir + "/" + fn;
      break;
    }
  }
  closedir(d);
  return result;
}

/*****************************************
 * addr_map.txt parser
 *   Each line: <element> <hex_addr> <hex_size>  (whitespace separated)
 *   Relative addresses are relocated to start_address using data_in as origin.
 *****************************************/
bool DrpAiRuntimeWrapper::Impl::ParseAddrMap(const std::string& path, uint64_t start_address) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) {
    log_err_ne("Failed to open addr_map.txt: " + path);
    return false;
  }

  struct Ent { uint64_t addr; uint64_t size; };
  std::unordered_map<std::string, Ent> ents;

  std::string line;
  while (std::getline(ifs, line)) {
    std::istringstream iss(line);
    std::string elem, addr_s, size_s;
    if (!(iss >> elem >> addr_s >> size_s)) continue;
    uint64_t addr = 0, size = 0;
    try {
      /* addr_map.txt uses hex WITHOUT the "0x" prefix (e.g. "1abb400").
       * Use base=16 explicitly, matching the reference PreRuntime.cpp
       * (strtol(..., 16)). base=0 would misread "1abb400" as decimal 1. */
      addr = std::stoull(addr_s, nullptr, 16);
      size = std::stoull(size_s, nullptr, 16);
    } catch (...) {
      continue;
    }
    ents[elem] = Ent{ addr, size };
  }
  ifs.close();

  auto it_din = ents.find("data_in");
  if (it_din == ents.end()) {
    log_err_ne("addr_map.txt does not contain data_in entry");
    return false;
  }
  const uint64_t data_in_addr = it_din->second.addr; /* addr_map origin */
  obj_.map_data_in_addr = data_in_addr;

  auto reloc = [&](const std::string& key, uint64_t& out_addr, uint64_t& out_size) {
    auto it = ents.find(key);
    if (it == ents.end()) return;
    out_addr = it->second.addr - data_in_addr + start_address;
    out_size = it->second.size;
  };

  reloc("drp_config",       obj_.drp_config_addr,       obj_.drp_config_size);
  reloc("drp_param",        obj_.drp_param_addr,        obj_.drp_param_size);
  reloc("aimac_desc",       obj_.desc_aimac_addr,       obj_.desc_aimac_size);
  reloc("drp_desc",         obj_.desc_drp_addr,         obj_.desc_drp_size);
  reloc("weight",           obj_.weight_addr,           obj_.weight_size);
  reloc("aimac_cmd",        obj_.aimac_cmd_addr,         obj_.aimac_cmd_size);
  reloc("aimac_param_desc", obj_.aimac_param_desc_addr, obj_.aimac_param_desc_size);
  reloc("aimac_param_cmd",  obj_.aimac_param_cmd_addr,  obj_.aimac_param_cmd_size);
  reloc("data_in",          obj_.data_in_addr,          obj_.data_in_size);
  reloc("data_out",         obj_.data_out_addr,         obj_.data_out_size);

  /* total object region size = highest (addr+size) relative to origin */
  uint64_t max_end = 0;
  for (const auto& kv : ents) {
    uint64_t end = (kv.second.addr - data_in_addr) + kv.second.size;
    if (end > max_end) max_end = end;
  }
  obj_.object_files_size = max_end;
  obj_.last_address = start_address + max_end;
  return true;
}

/*****************************************
 * prepost_*.yaml parser (lightweight, tailored to the known layout)
 *   Only 'input_to_pre' and 'output_from_post' sections are consumed.
 *****************************************/
bool DrpAiRuntimeWrapper::Impl::ParsePrepostYaml(const std::string& path) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) {
    log_err_ne("Failed to open prepost yaml: " + path);
    return false;
  }

  enum class Sec { NONE, INPUT_PRE, OUTPUT_POST, OTHER };
  Sec sec = Sec::NONE;

  auto section_of = [](const std::string& key) -> Sec {
    if (key == "input_to_pre")     return Sec::INPUT_PRE;
    if (key == "output_from_post") return Sec::OUTPUT_POST;
    if (key == "input_to_body" || key == "output_from_body" ||
        key == "preprocess" || key == "postprocess") return Sec::OTHER;
    return Sec::NONE; /* unknown top-level -> ignore section content */
  };

  /* parse a bracketed list "[a, b, c]" into int64 vector */
  auto parse_shape = [](const std::string& v) {
    std::vector<int64_t> out;
    std::string s = v;
    auto lb = s.find('[');
    auto rb = s.find(']');
    if (lb == std::string::npos || rb == std::string::npos || rb < lb) return out;
    s = s.substr(lb + 1, rb - lb - 1);
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      tok = trim(tok);
      if (tok.empty()) continue;
      try { out.push_back(std::stoll(tok)); } catch (...) {}
    }
    return out;
  };

  auto unquote = [](std::string v) {
    v = trim(v);
    if (v.size() >= 2 && (v.front() == '"' || v.front() == '\'') &&
        (v.back() == '"' || v.back() == '\'')) {
      v = v.substr(1, v.size() - 2);
    }
    return v;
  };

  std::vector<NodeInfo>* cur_vec = nullptr;
  NodeInfo* cur_node = nullptr;
  int base_indent = -1; /* indentation of the list item '-' inside a section */

  std::string line;
  while (std::getline(ifs, line)) {
    /* strip comments (# ...) */
    auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    if (trim(line).empty()) continue;

    /* compute indentation */
    size_t indent = line.find_first_not_of(" ");
    std::string content = trim(line);

    /* top-level key (indent == 0) */
    if (indent == 0) {
      /* "key:" */
      std::string key = content;
      if (!key.empty() && key.back() == ':') key.pop_back();
      key = trim(key);
      sec = section_of(key);
      cur_vec = (sec == Sec::INPUT_PRE) ? &inputs_
              : (sec == Sec::OUTPUT_POST) ? &outputs_ : nullptr;
      cur_node = nullptr;
      base_indent = -1;
      continue;
    }

    if (!cur_vec) continue; /* section we don't care about */

    /* list item start: content begins with '-' */
    if (content[0] == '-') {
      cur_vec->emplace_back();
      cur_node = &cur_vec->back();
      base_indent = static_cast<int>(indent);
      /* possible inline "- name: xxx" */
      std::string rest = trim(content.substr(1));
      if (rest.empty()) continue;
      content = rest; /* fallthrough to key:value parse below */
    }

    if (!cur_node) continue;

    /* key: value */
    auto colon = content.find(':');
    if (colon == std::string::npos) continue;
    std::string key = trim(content.substr(0, colon));
    std::string val = trim(content.substr(colon + 1));

    if (key == "name") {
      cur_node->name = unquote(val);
    } else if (key == "shape") {
      cur_node->shape = parse_shape(val);
    } else if (key == "order") {
      cur_node->order = unquote(val);
    } else if (key == "type") {
      std::string t = unquote(val);
      cur_node->dtype = type_from_string(t);
      cur_node->elem_size = elem_size_from_string(t);
    }
  }
  ifs.close();

  /* finalize sizes */
  auto finalize = [](std::vector<NodeInfo>& v) {
    for (auto& n : v) {
      size_t ne = 1;
      for (auto d : n.shape) ne *= static_cast<size_t>(d <= 0 ? 1 : d);
      n.num_elem  = n.shape.empty() ? 0 : ne;
      n.bytes_size = n.num_elem * n.elem_size;
    }
  };
  finalize(inputs_);
  finalize(outputs_);

  if (inputs_.empty()) {
    log_err_ne("prepost yaml: no input_to_pre node found");
    return false;
  }
  if (outputs_.empty()) {
    log_err_ne("prepost yaml: no output_from_post node found");
    return false;
  }
  return true;
}

/*****************************************
 * data_in_list.txt / data_out_list.txt parser
 *   Blocks:
 *     Input_node_name: <name>   (or Output_node_name)
 *              Address: 0x....
 *              Channel: C
 *              Width  : W
 *              Height : H
 *   Matched to inputs_/outputs_ by node name.
 *****************************************/
bool DrpAiRuntimeWrapper::Impl::ParseDataList(const std::string& path, bool is_input,
                                             uint64_t start_address) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) {
    log_err_ne("Failed to open data list: " + path);
    return false;
  }

  std::vector<NodeInfo>& target = is_input ? inputs_ : outputs_;

  std::string line;
  std::string cur_name;
  uint64_t    cur_addr = 0;
  uint64_t    cur_c = 0, cur_w = 0, cur_h = 0;
  bool        have = false;

  auto commit = [&]() {
    if (!have) return;
    NodeInfo* n = FindNode(target, cur_name);
    if (n) {
      /* data_*_list.txt Address is a RELATIVE address (origin 0, same base as
       * addr_map.txt data_in). Relocate to the actual placed address by adding
       * start_address, consistent with ParseAddrMap()'s object relocation. */
      n->phys_addr = cur_addr + start_address;
      n->addr_valid = true;
      /* size cross-check (C*W*H*elem vs yaml bytes) */
      uint64_t list_bytes = cur_c * cur_w * cur_h * static_cast<uint64_t>(n->elem_size);
      if (list_bytes != 0 && n->bytes_size != 0 && list_bytes != n->bytes_size) {
        log_warn("Size mismatch for node '" + cur_name + "': yaml=" +
                 std::to_string(n->bytes_size) + " list=" + std::to_string(list_bytes));
      }
    } else {
      log_warn("data list node not found in yaml: " + cur_name);
    }
    have = false;
    cur_c = cur_w = cur_h = 0;
  };

  while (std::getline(ifs, line)) {
    std::string c = trim(line);
    if (c.empty()) continue;
    auto colon = c.find(':');
    if (colon == std::string::npos) continue;
    std::string key = trim(c.substr(0, colon));
    std::string val = trim(c.substr(colon + 1));

    if (key == "Input_node_name" || key == "Output_node_name") {
      commit();
      cur_name = val;
      have = true;
    } else if (key == "Address") {
      /* data_*_list.txt uses hex WITHOUT the "0x" prefix, same as addr_map.txt.
       * Use base=16 explicitly; base=0 would misread e.g. "49800" as decimal. */
      try { cur_addr = std::stoull(val, nullptr, 16); } catch (...) { cur_addr = 0; }
    } else if (key == "Channel") {
      try { cur_c = std::stoull(val, nullptr, 0); } catch (...) {}
    } else if (key == "Width") {
      try { cur_w = std::stoull(val, nullptr, 0); } catch (...) {}
    } else if (key == "Height") {
      try { cur_h = std::stoull(val, nullptr, 0); } catch (...) {}
    }
  }
  commit();
  ifs.close();

  /* verify all target nodes have an address */
  for (const auto& n : target) {
    if (!n.addr_valid) {
      log_err_ne("No physical address found for node: " + n.name);
      return false;
    }
  }
  return true;
}

/*****************************************
 * Load one object file to DRP-AI memory (ASSIGN + write).
 *****************************************/
bool DrpAiRuntimeWrapper::Impl::LoadOneFile(const std::string& file, uint64_t addr, uint64_t size) {
  if (size == 0) {
    /* nothing to load (element may be absent) */
    return true;
  }

  int obj_fd = ::open(file.c_str(), O_RDONLY);
  if (obj_fd < 0) {
    log_err("Failed to open object file: " + file);
    return false;
  }

  drpai_data_t d{};
  d.address = addr;
  d.size    = size;
  errno = 0;
  if (ioctl(drpai_fd_, DRPAI_ASSIGN, &d) != 0) {
    char detail[256];
    std::snprintf(detail, sizeof(detail),
      "DRPAI_ASSIGN failed for %s (addr=0x%llx size=0x%llx addr%%64=%llu)",
      file.c_str(),
      (unsigned long long)addr, (unsigned long long)size,
      (unsigned long long)(addr % 64));
    log_err(detail);
    ::close(obj_fd);
    return false;
  }

  std::vector<uint8_t> buf(BUF_SIZE);
  uint64_t remaining = size;
  while (remaining > 0) {
    size_t chunk = static_cast<size_t>(std::min<uint64_t>(BUF_SIZE, remaining));
    ssize_t rd = ::read(obj_fd, buf.data(), chunk);
    if (rd <= 0) {
      log_err("read from object file failed: " + file);
      ::close(obj_fd);
      return false;
    }
    ssize_t wr = ::write(drpai_fd_, buf.data(), static_cast<size_t>(rd));
    if (wr != rd) {
      log_err("write to DRP-AI failed: " + file);
      ::close(obj_fd);
      return false;
    }
    remaining -= static_cast<uint64_t>(rd);
  }
  ::close(obj_fd);
  return true;
}

/*****************************************
 * Load object body files (8 files).
 *****************************************/
bool DrpAiRuntimeWrapper::Impl::LoadDrpaiData(const std::string& model_dir) {
  struct FileMap { const char* fname; uint64_t addr; uint64_t size; };
  const FileMap files[] = {
    { "drp_desc.bin",         obj_.desc_drp_addr,          obj_.desc_drp_size },
    { "drp_config.mem",       obj_.drp_config_addr,        obj_.drp_config_size },
    { "drp_param.bin",        obj_.drp_param_addr,         obj_.drp_param_size },
    { "aimac_desc.bin",       obj_.desc_aimac_addr,        obj_.desc_aimac_size },
    { "weight.bin",           obj_.weight_addr,            obj_.weight_size },
    { "aimac_cmd.bin",        obj_.aimac_cmd_addr,         obj_.aimac_cmd_size },
    { "aimac_param_desc.bin", obj_.aimac_param_desc_addr,  obj_.aimac_param_desc_size },
    { "aimac_param_cmd.bin",  obj_.aimac_param_cmd_addr,   obj_.aimac_param_cmd_size },
  };
  for (const auto& f : files) {
    std::string path = model_dir + "/" + f.fname;
    if (!LoadOneFile(path, f.addr, f.size)) {
      return false;
    }
  }
  return true;
}

/*****************************************
 * Load parameter file (ASSIGN_PARAM + write).
 *****************************************/
bool DrpAiRuntimeWrapper::Impl::LoadParamFile(const std::string& model_dir) {
  const std::string path = model_dir + "/drp_param_info.txt";
  int pfd = ::open(path.c_str(), O_RDONLY);
  if (pfd < 0) {
    log_err("Failed to open drp_param_info.txt");
    return false;
  }
  off_t fsize = lseek(pfd, 0, SEEK_END);
  lseek(pfd, 0, SEEK_SET);
  if (fsize < 0) {
    log_err("lseek on drp_param_info.txt failed");
    ::close(pfd);
    return false;
  }

  drpai_assign_param_t param{};
  param.info_size   = static_cast<uint32_t>(fsize);
  param.obj.address = obj_.drp_param_addr;
  param.obj.size    = obj_.drp_param_size;

  errno = 0;
  if (ioctl(drpai_fd_, DRPAI_ASSIGN_PARAM, &param) != 0) {
    log_err("DRPAI_ASSIGN_PARAM failed");
    ::close(pfd);
    return false;
  }

  std::vector<uint8_t> buf(BUF_SIZE);
  off_t remaining = fsize;
  while (remaining > 0) {
    size_t chunk = static_cast<size_t>(std::min<off_t>(BUF_SIZE, remaining));
    ssize_t rd = ::read(pfd, buf.data(), chunk);
    if (rd <= 0) {
      log_err("read from drp_param_info.txt failed");
      ::close(pfd);
      return false;
    }
    ssize_t wr = ::write(drpai_fd_, buf.data(), static_cast<size_t>(rd));
    if (wr != rd) {
      log_err("write drp_param_info.txt to DRP-AI failed");
      ::close(pfd);
      return false;
    }
    remaining -= rd;
  }
  ::close(pfd);
  return true;
}

/*****************************************
 * LoadModel
 *****************************************/
bool DrpAiRuntimeWrapper::Impl::LoadModel(const std::string& model_dir, uint64_t start_address) {
  start_address_ = start_address;

  /* (1) open driver */
  errno = 0;
  drpai_fd_ = ::open(DRPAI_DEV, O_RDWR);
  if (drpai_fd_ < 0) {
    log_err("Failed to open DRP-AI Driver");
    return false;
  }

  /* (2) get DRP-AI memory area */
  errno = 0;
  if (ioctl(drpai_fd_, DRPAI_GET_DRPAI_AREA, &drpai_area_) == -1) {
    log_err("DRPAI_GET_DRPAI_AREA failed");
    CloseDriver();
    return false;
  }

  /* (2b) validate start_address against DRP-AI memory area */
  {
    const uint64_t area_beg = drpai_area_.address;
    const uint64_t area_end = drpai_area_.address + drpai_area_.size; /* exclusive */
    char dbg[192];
    std::snprintf(dbg, sizeof(dbg),
      "DRP-AI area: addr=0x%llx size=0x%llx (end=0x%llx), start_address=0x%llx",
      (unsigned long long)area_beg, (unsigned long long)drpai_area_.size,
      (unsigned long long)area_end, (unsigned long long)start_address);
    log_info(dbg);
    if (start_address < area_beg || start_address >= area_end) {
      log_err_ne("start_address is out of DRP-AI memory area");
      CloseDriver();
      return false;
    }
    if ((start_address % 64) != 0) {
      log_err_ne("start_address is not 64-byte aligned");
      CloseDriver();
      return false;
    }
  }

  /* (3) parse addr_map.txt (relocate) */
  if (!ParseAddrMap(model_dir + "/addr_map.txt", start_address)) {
    CloseDriver();
    return false;
  }

  /* (3b) verify the whole object region fits within the DRP-AI area */
  {
    const uint64_t region_end = start_address + obj_.object_files_size; /* exclusive */
    const uint64_t area_end    = drpai_area_.address + drpai_area_.size;
    char dbg[192];
    std::snprintf(dbg, sizeof(dbg),
      "object region: start=0x%llx size=0x%llx (end=0x%llx), area_end=0x%llx",
      (unsigned long long)start_address, (unsigned long long)obj_.object_files_size,
      (unsigned long long)region_end, (unsigned long long)area_end);
    log_info(dbg);
    if (region_end > area_end) {
      log_err_ne("object region exceeds DRP-AI memory area (start_address too high or model too large)");
      CloseDriver();
      return false;
    }
  }

  /* (4) parse prepost yaml (inputs_/outputs_ meta) */
  std::string yaml = FindFileByPrefixSuffix(model_dir, "prepost_", ".yaml");
  if (yaml.empty()) yaml = FindFileBySuffix(model_dir, ".yaml");
  if (yaml.empty()) {
    log_err_ne("prepost_*.yaml not found in model dir");
    CloseDriver();
    return false;
  }
  if (!ParsePrepostYaml(yaml)) {
    CloseDriver();
    return false;
  }

  /* (5) parse data_in_list.txt / data_out_list.txt (phys addresses) */
  if (!ParseDataList(model_dir + "/data_in_list.txt", true, start_address)) {
    CloseDriver();
    return false;
  }
  if (!ParseDataList(model_dir + "/data_out_list.txt", false, start_address)) {
    CloseDriver();
    return false;
  }

  /* (6) load object body files */
  if (!LoadDrpaiData(model_dir)) {
    CloseDriver();
    return false;
  }

  /* (7) load parameter file */
  if (!LoadParamFile(model_dir)) {
    CloseDriver();
    return false;
  }

  /* (8) prepare output host buffers */
  output_host_buf_.assign(outputs_.size(), std::vector<uint8_t>());

  loaded_ = true;
  log_info("LoadModel succeeded: " + model_dir);
  return true;
}

/*****************************************
 * SetInput
 *****************************************/
template <typename T>
void DrpAiRuntimeWrapper::Impl::SetInputImpl(int input_index, const T* data_ptr) {
  if (!loaded_) {
    log_err_ne("SetInput called before LoadModel");
    return;
  }
  if (input_index < 0 || input_index >= static_cast<int>(inputs_.size())) {
    log_err_ne("SetInput: input_index out of range");
    return;
  }
  NodeInfo& n = inputs_[input_index];
  if (!n.addr_valid) {
    log_err_ne("SetInput: node has no physical address: " + n.name);
    return;
  }

  drpai_data_t d{};
  d.address = n.phys_addr;   /* already relocated (relative + start_address) */
  d.size    = n.bytes_size;
  errno = 0;
  if (ioctl(drpai_fd_, DRPAI_ASSIGN, &d) != 0) {
    char detail[256];
    std::snprintf(detail, sizeof(detail),
      "SetInput: DRPAI_ASSIGN failed for %s (addr=0x%llx size=0x%llx addr%%64=%llu)",
      n.name.c_str(),
      (unsigned long long)d.address, (unsigned long long)d.size,
      (unsigned long long)(d.address % 64));
    log_err(detail);
    return;
  }

  const uint8_t* src = reinterpret_cast<const uint8_t*>(data_ptr);
  uint64_t remaining = n.bytes_size;
  while (remaining > 0) {
    size_t chunk = static_cast<size_t>(std::min<uint64_t>(BUF_SIZE, remaining));
    ssize_t wr = ::write(drpai_fd_, src, chunk);
    if (wr <= 0) {
      log_err("SetInput: write to DRP-AI failed for " + n.name);
      return;
    }
    src       += wr;
    remaining -= static_cast<uint64_t>(wr);
  }
}

/*****************************************
 * GetNumInput
 *****************************************/
int DrpAiRuntimeWrapper::Impl::GetNumInput(const std::string& model_dir) {
  if (!inputs_.empty()) {
    return static_cast<int>(inputs_.size());
  }
  /* Not loaded yet: parse yaml to count inputs. */
  std::string yaml = FindFileByPrefixSuffix(model_dir, "prepost_", ".yaml");
  if (yaml.empty()) yaml = FindFileBySuffix(model_dir, ".yaml");
  if (yaml.empty()) {
    log_err_ne("GetNumInput: prepost yaml not found");
    return 0;
  }
  if (!ParsePrepostYaml(yaml)) {
    return 0;
  }
  return static_cast<int>(inputs_.size());
}

/*****************************************
 * GetInputDataType
 *****************************************/
InOutDataType DrpAiRuntimeWrapper::Impl::GetInputDataType(int index) {
  if (index < 0 || index >= static_cast<int>(inputs_.size())) {
    log_err_ne("GetInputDataType: index out of range");
    return InOutDataType::OTHER;
  }
  return inputs_[index].dtype;
}

/*****************************************
 * GetOutput (ASSIGN + read)
 *****************************************/
std::tuple<InOutDataType, void*, int64_t>
DrpAiRuntimeWrapper::Impl::GetOutput(int index) {
  if (index < 0 || index >= static_cast<int>(outputs_.size())) {
    log_err_ne("GetOutput: index out of range");
    return std::make_tuple(InOutDataType::OTHER, nullptr, 0);
  }
  NodeInfo& n = outputs_[index];
  if (!n.addr_valid) {
    log_err_ne("GetOutput: node has no physical address: " + n.name);
    return std::make_tuple(InOutDataType::OTHER, nullptr, 0);
  }

  std::vector<uint8_t>& host = output_host_buf_[index];
  host.assign(n.bytes_size, 0);

  drpai_data_t d{};
  d.address = n.phys_addr;
  d.size    = n.bytes_size;
  errno = 0;
  if (ioctl(drpai_fd_, DRPAI_ASSIGN, &d) != 0) {
    log_err("GetOutput: DRPAI_ASSIGN failed for " + n.name);
    return std::make_tuple(n.dtype, nullptr, 0);
  }

  uint8_t* dst = host.data();
  uint64_t remaining = n.bytes_size;
  while (remaining > 0) {
    size_t chunk = static_cast<size_t>(std::min<uint64_t>(BUF_SIZE, remaining));
    ssize_t rd = ::read(drpai_fd_, dst, chunk);
    if (rd <= 0) {
      log_err("GetOutput: read from DRP-AI failed for " + n.name);
      return std::make_tuple(n.dtype, nullptr, 0);
    }
    dst       += rd;
    remaining -= static_cast<uint64_t>(rd);
  }

  return std::make_tuple(n.dtype, reinterpret_cast<void*>(host.data()),
                         static_cast<int64_t>(n.num_elem));
}

/*****************************************
 * Zero-copy API
 *   NOTE: DRP-AI memory is physical. Direct pointer access requires mmap.
 *   In this PoC, GetInputPtr/GetOutputPtr return host staging buffers
 *   (spec section 4.11, approach B), which are synchronized via
 *   SetInput/Run/GetOutput. name mismatch -> nullptr.
 *****************************************/
std::vector<std::tuple<std::string, size_t, InOutDataType>>
DrpAiRuntimeWrapper::Impl::GetInputInfo() {
  std::vector<std::tuple<std::string, size_t, InOutDataType>> v;
  for (const auto& n : inputs_) {
    v.emplace_back(n.name, n.bytes_size, n.dtype);
  }
  return v;
}

void* DrpAiRuntimeWrapper::Impl::GetInputPtr(const std::string& name) {
  for (size_t i = 0; i < inputs_.size(); ++i) {
    if (inputs_[i].name == name) {
      /* lazily allocate a host staging buffer sized to the node */
      if (input_staging_.size() < inputs_.size()) input_staging_.resize(inputs_.size());
      if (input_staging_[i].size() != inputs_[i].bytes_size) {
        input_staging_[i].assign(inputs_[i].bytes_size, 0);
      }
      return reinterpret_cast<void*>(input_staging_[i].data());
    }
  }
  log_warn("GetInputPtr: name not found: " + name);
  return nullptr;
}

std::vector<std::tuple<std::string, size_t, InOutDataType>>
DrpAiRuntimeWrapper::Impl::GetOutputInfo() {
  std::vector<std::tuple<std::string, size_t, InOutDataType>> v;
  for (const auto& n : outputs_) {
    v.emplace_back(n.name, n.bytes_size, n.dtype);
  }
  return v;
}

void* DrpAiRuntimeWrapper::Impl::GetOutputPtr(const std::string& name) {
  for (size_t i = 0; i < outputs_.size(); ++i) {
    if (outputs_[i].name == name) {
      if (output_host_buf_[i].size() != outputs_[i].bytes_size) {
        output_host_buf_[i].assign(outputs_[i].bytes_size, 0);
      }
      return reinterpret_cast<void*>(output_host_buf_[i].data());
    }
  }
  log_warn("GetOutputPtr: name not found: " + name);
  return nullptr;
}

/*****************************************
 * Setup address conversion table (DRPAI_SET_ADRCONV)
 *****************************************/
bool DrpAiRuntimeWrapper::Impl::SetupAdrConv() {
  /* (1) object files region: REPLACE (clear then register) */
  drpai_adrconv_t addr_info{};
  addr_info.org_address  = obj_.map_data_in_addr;         /* addr_map origin       */
  addr_info.size         = obj_.object_files_size;        /* total object size     */
  addr_info.conv_address = start_address_;                /* actual placed address */
  addr_info.mode         = DRPAI_ADRCONV_MODE_REPLACE;
  errno = 0;
  if (ioctl(drpai_fd_, DRPAI_SET_ADRCONV, &addr_info) != 0) {
    log_err("DRPAI_SET_ADRCONV (object region) failed");
    return false;
  }
  return true;
}

/*****************************************
 * Setup proc[] array for DRPAI_START
 *****************************************/
void DrpAiRuntimeWrapper::Impl::SetupProc(drpai_data_t* proc) {
  std::memset(proc, 0, sizeof(drpai_data_t) * DRPAI_INDEX_NUM);

  /* input: use physical address from data_in_list.txt (first input node) */
  uint64_t in_addr = (!inputs_.empty() && inputs_[0].addr_valid)
                       ? inputs_[0].phys_addr : obj_.data_in_addr;
  uint64_t in_size = (!inputs_.empty()) ? inputs_[0].bytes_size : obj_.data_in_size;

  proc[DRPAI_INDEX_INPUT].address            = in_addr;
  proc[DRPAI_INDEX_INPUT].size               = in_size;
  proc[DRPAI_INDEX_DRP_CFG].address          = obj_.drp_config_addr;
  proc[DRPAI_INDEX_DRP_CFG].size             = obj_.drp_config_size;
  proc[DRPAI_INDEX_DRP_PARAM].address        = obj_.drp_param_addr;
  proc[DRPAI_INDEX_DRP_PARAM].size           = obj_.drp_param_size;
  proc[DRPAI_INDEX_AIMAC_DESC].address       = obj_.desc_aimac_addr;
  proc[DRPAI_INDEX_AIMAC_DESC].size          = obj_.desc_aimac_size;
  proc[DRPAI_INDEX_DRP_DESC].address         = obj_.desc_drp_addr;
  proc[DRPAI_INDEX_DRP_DESC].size            = obj_.desc_drp_size;
  proc[DRPAI_INDEX_WEIGHT].address           = obj_.weight_addr;
  proc[DRPAI_INDEX_WEIGHT].size              = obj_.weight_size;
  proc[DRPAI_INDEX_OUTPUT].address           = obj_.data_out_addr;
  proc[DRPAI_INDEX_OUTPUT].size              = obj_.data_out_size;
  proc[DRPAI_INDEX_AIMAC_CMD].address        = obj_.aimac_cmd_addr;
  proc[DRPAI_INDEX_AIMAC_CMD].size           = obj_.aimac_cmd_size;
  proc[DRPAI_INDEX_AIMAC_PARAM_DESC].address = obj_.aimac_param_desc_addr;
  proc[DRPAI_INDEX_AIMAC_PARAM_DESC].size    = obj_.aimac_param_desc_size;
  proc[DRPAI_INDEX_AIMAC_PARAM_CMD].address  = obj_.aimac_param_cmd_addr;
  proc[DRPAI_INDEX_AIMAC_PARAM_CMD].size     = obj_.aimac_param_cmd_size;
}

/*****************************************
 * Run
 *****************************************/
void DrpAiRuntimeWrapper::Impl::Run(int freq_index) {
  if (!loaded_) {
    log_err_ne("Run called before LoadModel");
    return;
  }

  /* (1) frequency settings */
  errno = 0;
  if (ioctl(drpai_fd_, DRPAI_SET_DRP_MAX_FREQ, &drp_max_freq_) != 0) {
    log_err("DRPAI_SET_DRP_MAX_FREQ failed");
    return;
  }
  uint32_t divfix = static_cast<uint32_t>(freq_index < 1 ? 1 : (freq_index > 127 ? 127 : freq_index));
  errno = 0;
  if (ioctl(drpai_fd_, DRPAI_SET_DRPAI_FREQ, &divfix) != 0) {
    log_err("DRPAI_SET_DRPAI_FREQ failed");
    return;
  }

  /* (2) address conversion */
  if (!SetupAdrConv()) {
    return;
  }

  /* (3) proc[] setup and START */
  drpai_data_t proc[DRPAI_INDEX_NUM];
  SetupProc(proc);

  errno = 0;
  if (ioctl(drpai_fd_, DRPAI_START, &proc[0]) != 0) {
    log_err("DRPAI_START failed");
    return;
  }

  /* (4) wait for completion */
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(drpai_fd_, &rfds);
  struct timespec tv;
  tv.tv_sec  = DRPAI_TIMEOUT;
  tv.tv_nsec = 0;

  errno = 0;
  int ret = pselect(drpai_fd_ + 1, &rfds, nullptr, nullptr, &tv, nullptr);
  if (ret == 0) {
    log_err("DRP-AI Inference pselect() timeout");
    return;
  } else if (ret < 0) {
    drpai_status_t st{};
    ioctl(drpai_fd_, DRPAI_GET_STATUS, &st);
    log_err("DRP-AI Inference pselect() error");
    return;
  }

  /* (5) completion check */
  drpai_status_t st{};
  errno = 0;
  int inf_status = ioctl(drpai_fd_, DRPAI_GET_STATUS, &st);
  if (inf_status != 0) {
    log_err("DRP-AI internal error (DRPAI_GET_STATUS)");
    return;
  }
  /* success: output is read on GetOutput() */
}

/*****************************************
 * DrpAiRuntimeWrapper (public wrapper)
 *****************************************/
DrpAiRuntimeWrapper::DrpAiRuntimeWrapper(int device_type)
    : impl_(new Impl(device_type)), device_type_(device_type) {}

#ifdef KDLCPUMODE
DrpAiRuntimeWrapper::DrpAiRuntimeWrapper()
    : impl_(new Impl(0 /* kDLCPU: not supported by DRP-AI */)), device_type_(0) {
  std::fprintf(stderr,
    "[WARN ][DrpAiRuntimeWrapper] CPU mode is not supported by DRP-AI Driver.\n");
}
#else
DrpAiRuntimeWrapper::DrpAiRuntimeWrapper()
    : impl_(new Impl(1 /* DRP-AI */)), device_type_(1) {}
#endif

DrpAiRuntimeWrapper::~DrpAiRuntimeWrapper() = default;

bool DrpAiRuntimeWrapper::LoadModel(const std::string& model_dir, uint64_t start_address) {
  return impl_->LoadModel(model_dir, start_address);
}

uint64_t DrpAiRuntimeWrapper::GetLastAddress() {
  return impl_->GetLastAddress();
}

void DrpAiRuntimeWrapper::SetInput(int input_index, const float* data_ptr) {
  impl_->SetInputImpl(input_index, data_ptr);
}

void DrpAiRuntimeWrapper::SetInput(int input_index, const uint16_t* data_ptr) {
  impl_->SetInputImpl(input_index, data_ptr);
}

int DrpAiRuntimeWrapper::GetNumInput(std::string model_dir) {
  return impl_->GetNumInput(model_dir);
}

InOutDataType DrpAiRuntimeWrapper::GetInputDataType(int index) {
  return impl_->GetInputDataType(index);
}

int DrpAiRuntimeWrapper::GetNumOutput() {
  return impl_->GetNumOutput();
}

std::tuple<InOutDataType, void*, int64_t> DrpAiRuntimeWrapper::GetOutput(int index) {
  return impl_->GetOutput(index);
}

std::vector<std::tuple<std::string, size_t, InOutDataType>> DrpAiRuntimeWrapper::GetInputInfo() {
  return impl_->GetInputInfo();
}

void* DrpAiRuntimeWrapper::GetInputPtr(const std::string& name) {
  return impl_->GetInputPtr(name);
}

std::vector<std::tuple<std::string, size_t, InOutDataType>> DrpAiRuntimeWrapper::GetOutputInfo() {
  return impl_->GetOutputInfo();
}

void* DrpAiRuntimeWrapper::GetOutputPtr(const std::string& name) {
  return impl_->GetOutputPtr(name);
}

void DrpAiRuntimeWrapper::Run(int freq_index) {
  impl_->Run(freq_index);
}

void DrpAiRuntimeWrapper::Run() {
  impl_->Run(1 /* freq_index = 1 */);
}

/* ProfileRun: NOT supported in v01 (spec section 4.9). Stub only. */
void DrpAiRuntimeWrapper::ProfileRun(const std::string& /*profile_table*/,
                                     const std::string& /*profile_csv*/) {
  std::fprintf(stderr,
    "[WARN ][DrpAiRuntimeWrapper] ProfileRun is not supported in v01.\n");
}

void DrpAiRuntimeWrapper::ProfileRun(const std::string& /*profile_table*/,
                                     const std::string& /*profile_csv*/,
                                     int /*freq_index*/) {
  std::fprintf(stderr,
    "[WARN ][DrpAiRuntimeWrapper] ProfileRun is not supported in v01.\n");
}