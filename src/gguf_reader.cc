#include "gguf_reader.h"

#include <format>
#include <fstream>
#include <sstream>
#include <string>
template <typename T> static bool read_binary(std::ifstream &fs, T &val) {
  fs.read(reinterpret_cast<char *>(&val), sizeof(T));
  return fs.gcount() == sizeof(T);
}

// Helper: Read GGUF packed strings (length as uint64_t followed by data bytes)
static std::string read_gguf_string(std::ifstream &fs) {
  uint64_t len = 0;
  if (!read_binary(fs, len) || len == 0)
    return "";

  std::string str(len, '\0');
  fs.read(&str[0], len);
  return str;
}

// Map official GGUF Metadata Value Types to printable strings
std::string gguf_type_to_str(gguf_metadata_value_type type) {
  switch (type) {
  case GGUF_METADATA_VALUE_TYPE_UINT8:
    return "UINT8";
  case GGUF_METADATA_VALUE_TYPE_INT8:
    return "INT8";
  case GGUF_METADATA_VALUE_TYPE_UINT16:
    return "UINT16";
  case GGUF_METADATA_VALUE_TYPE_INT16:
    return "INT16";
  case GGUF_METADATA_VALUE_TYPE_UINT32:
    return "UINT32";
  case GGUF_METADATA_VALUE_TYPE_INT32:
    return "INT32";
  case GGUF_METADATA_VALUE_TYPE_FLOAT32:
    return "FLOAT32";
  case GGUF_METADATA_VALUE_TYPE_BOOL:
    return "BOOL";
  case GGUF_METADATA_VALUE_TYPE_STRING:
    return "STRING";
  case GGUF_METADATA_VALUE_TYPE_ARRAY:
    return "ARRAY";
  case GGUF_METADATA_VALUE_TYPE_UINT64:
    return "UINT64";
  case GGUF_METADATA_VALUE_TYPE_INT64:
    return "INT64";
  case GGUF_METADATA_VALUE_TYPE_FLOAT64:
    return "FLOAT64";
  default:
    return "UNKNOWN";
  }
}

// Map GGML Quantization types to printable strings
std::string ggml_type_to_str(ggml_type type) {
  switch (type) {
  case GGML_TYPE_F32:
    return "FP32";
  case GGML_TYPE_F16:
    return "FP16";
  case GGML_TYPE_Q4_0:
    return "Q4_0";
  case GGML_TYPE_Q4_1:
    return "Q4_1";
  case GGML_TYPE_Q5_0:
    return "Q5_0";
  case GGML_TYPE_Q5_1:
    return "Q5_1";
  case GGML_TYPE_Q8_0:
    return "Q8_0";
  case GGML_TYPE_Q8_1:
    return "Q8_1";
  case GGML_TYPE_Q2_K:
    return "Q2_K";
  case GGML_TYPE_Q3_K:
    return "Q3_K";
  case GGML_TYPE_Q4_K:
    return "Q4_K";
  case GGML_TYPE_Q5_K:
    return "Q5_K";
  case GGML_TYPE_Q6_K:
    return "Q6_K";
  case GGML_TYPE_Q8_K:
    return "Q8_K";
  case GGML_TYPE_IQ2_XXS:
    return "IQ2_XXS";
  case GGML_TYPE_IQ2_XS:
    return "IQ2_XS";
  case GGML_TYPE_IQ3_XXS:
    return "IQ3_XXS";
  case GGML_TYPE_IQ1_S:
    return "IQ1_S";
  case GGML_TYPE_IQ4_NL:
    return "IQ4_NL";
  case GGML_TYPE_IQ3_S:
    return "IQ3_S";
  case GGML_TYPE_IQ2_S:
    return "IQ2_S";
  case GGML_TYPE_IQ4_XS:
    return "IQ4_XS";
  case GGML_TYPE_I8:
    return "INT8";
  case GGML_TYPE_I16:
    return "INT16";
  case GGML_TYPE_I32:
    return "INT32";
  case GGML_TYPE_I64:
    return "INT64";
  case GGML_TYPE_F64:
    return "FP64";
  case GGML_TYPE_IQ1_M:
    return "IQ1_M";
  case GGML_TYPE_BF16:
    return "BF16";
  case GGML_TYPE_TQ1_0:
    return "TQ1_0";
  case GGML_TYPE_TQ2_0:
    return "TQ2_0";
  case GGML_TYPE_MXFP4:
    return "MXFP4";
  default:
    return "UNKNOWN";
  }
}

static std::string parse_kv_value(std::ifstream &fs, uint32_t type_id,
                                  GGUFMetadataRecord &record);

void GGUFReader::clear() {
  metadata_list.clear();
  tensor_list.clear();
}

bool GGUFReader::skim_file(const std::string &file_path) {
  std::ifstream fs(file_path, std::ios::binary);
  if (!fs.is_open())
    return false;

  clear();

  // 1. Parse Magic and Version Setup
  char magic[4];
  fs.read(magic, 4);
  if (magic[0] != 'G' || magic[1] != 'G' || magic[2] != 'U' ||
      magic[3] != 'F') {
    return false; // Not a valid GGUF file
  }

  uint32_t version = 0;
  if (!read_binary(fs, version))
    return false;
  file_version = version; // Store it inside the instance class container

  uint64_t tensor_count = 0;
  uint64_t metadata_kv_count = 0;
  if (!read_binary(fs, tensor_count) || !read_binary(fs, metadata_kv_count))
    return false;



  // 2. Loop and read Metadata Key-Value pairs
  for (uint64_t i = 0; i < metadata_kv_count; ++i) {
    GGUFMetadataRecord record;
    record.key = read_gguf_string(fs);

    uint32_t type_id = 0;
    if (!read_binary(fs, type_id))
      return false;
    record.type = static_cast<gguf_metadata_value_type>(type_id);
    record.is_array = (record.type == GGUF_METADATA_VALUE_TYPE_ARRAY);

    record.value_str = parse_kv_value(fs, type_id, record);
    metadata_list.push_back(record);
  }

  // 3. Loop and read Tensor Information entries (skimming layout offsets)
  for (uint64_t i = 0; i < tensor_count; ++i) {
    GGUFTensorRecord tensor;
    tensor.name = read_gguf_string(fs);

    uint32_t dimensions_count = 0;
    if (!read_binary(fs, dimensions_count))
      return false;

    uint64_t total_elements = (dimensions_count > 0) ? 1 : 0;
    for (uint32_t d = 0; d < dimensions_count; ++d) {
      uint64_t dim_size = 0;
      if (!read_binary(fs, dim_size))
        return false;
      tensor.shape.push_back(dim_size);
      total_elements *= dim_size;
    }
    tensor.element_count = total_elements;

    uint32_t ggml_type_id = 0;
    if (!read_binary(fs, ggml_type_id))
      return false;
    tensor.type = static_cast<ggml_type>(ggml_type_id);

    uint64_t offset = 0;
    if (!read_binary(fs, offset))
      return false; // Skim file pointer block positioning

    // Estimate size approximations dynamically based on weights format bounds
    // (For unquantized formats we evaluate bytes directly, quantized uses block
    // logic)
    if (tensor.type == GGML_TYPE_F32)
      tensor.size_bytes = total_elements * 4;
    else if (tensor.type == GGML_TYPE_F16)
      tensor.size_bytes = total_elements * 2;
    else if (tensor.type == GGML_TYPE_BF16)
      tensor.size_bytes = total_elements * 2;
    else {
      // Quantized approximations fallback (Typically ~0.5 to 1.0 bytes per
      // element)
      tensor.size_bytes = total_elements * 0.56;
    }

    tensor_list.push_back(tensor);
  }

  return true;
}

static std::string parse_kv_value(std::ifstream& fs, uint32_t type_id, GGUFMetadataRecord& record) {
    std::stringstream ss;
    switch (type_id) {
        case GGUF_METADATA_VALUE_TYPE_UINT8:   { uint8_t v;  read_binary(fs, v); ss << (int)v; break; }
        case GGUF_METADATA_VALUE_TYPE_INT8:    {  int8_t v;  read_binary(fs, v); ss << (int)v; break; }
        case GGUF_METADATA_VALUE_TYPE_UINT16:  { uint16_t v; read_binary(fs, v); ss << v; break; }
        case GGUF_METADATA_VALUE_TYPE_INT16:   {  int16_t v; read_binary(fs, v); ss << v; break; }
        case GGUF_METADATA_VALUE_TYPE_UINT32:  { uint32_t v; read_binary(fs, v); ss << v; break; }
        case GGUF_METADATA_VALUE_TYPE_INT32:   {  int32_t v; read_binary(fs, v); ss << v; break; }
        case GGUF_METADATA_VALUE_TYPE_FLOAT32: {  float v;   read_binary(fs, v); ss << v; break; }
        case GGUF_METADATA_VALUE_TYPE_UINT64:  { uint64_t v; read_binary(fs, v); ss << v; break; }
        case GGUF_METADATA_VALUE_TYPE_INT64:   {  int64_t v; read_binary(fs, v); ss << v; break; }
        case GGUF_METADATA_VALUE_TYPE_FLOAT64: {  double v;  read_binary(fs, v); ss << v; break; }
        case GGUF_METADATA_VALUE_TYPE_BOOL:    {   bool v;   read_binary(fs, v); ss << (v ? "true" : "false"); break; }
        case GGUF_METADATA_VALUE_TYPE_STRING:  { ss << read_gguf_string(fs); break; }
        
        case GGUF_METADATA_VALUE_TYPE_ARRAY:   {
            uint32_t item_type = 0;
            uint64_t array_len = 0;
            read_binary(fs, item_type);
            read_binary(fs, array_len);
            
            record.is_array = true;
            record.array_elements_count = array_len;

            // Generate a readable summary value (e.g., "<STRING, 32000>")
            ss << "<" << gguf_type_to_str(static_cast<gguf_metadata_value_type>(item_type)) << ", " << array_len << ">";

            // Optimize vector storage allocation upfront
            record.array_items.reserve(array_len);

            if (item_type == GGUF_METADATA_VALUE_TYPE_STRING) {
                for (uint64_t idx = 0; idx < array_len; ++idx) {
                    record.array_items.push_back(read_gguf_string(fs));
                }
            } else {
                for (uint64_t idx = 0; idx < array_len; ++idx) {
                    switch (item_type) {
                        case GGUF_METADATA_VALUE_TYPE_UINT8:   { uint8_t v;  read_binary(fs, v); record.array_items.push_back(std::to_string(v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_INT8:    {  int8_t v;  read_binary(fs, v); record.array_items.push_back(std::to_string(v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_UINT16:  { uint16_t v; read_binary(fs, v); record.array_items.push_back(std::to_string(v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_INT16:   {  int16_t v; read_binary(fs, v); record.array_items.push_back(std::to_string(v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_UINT32:  { uint32_t v; read_binary(fs, v); record.array_items.push_back(std::to_string(v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_INT32:   {  int32_t v; read_binary(fs, v); record.array_items.push_back(std::to_string(v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_FLOAT32: {  float v;   read_binary(fs, v); record.array_items.push_back(std::format("{:.2f}", v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_UINT64:  { uint64_t v; read_binary(fs, v); record.array_items.push_back(std::to_string(v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_INT64:   {  int64_t v; read_binary(fs, v); record.array_items.push_back(std::to_string(v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_FLOAT64: {  double v;  read_binary(fs, v);  record.array_items.push_back(std::format("{:.2f}", v)); break; }
                        case GGUF_METADATA_VALUE_TYPE_BOOL:    {   bool v;   read_binary(fs, v); record.array_items.push_back(v ? "true" : "false"); break; }
                        default: break; 
                    }
                }
            }
            break;
        }
        default: ss << "[Binary Data Block]"; break;
    }
    return ss.str();
}