#pragma once
#include <stdint.h>
#include <string>
#include <vector>

// Updated to use official spec name and explicit underlying sizing
enum gguf_metadata_value_type : uint32_t {
    GGUF_METADATA_VALUE_TYPE_UINT8 = 0,
    GGUF_METADATA_VALUE_TYPE_INT8 = 1,
    GGUF_METADATA_VALUE_TYPE_UINT16 = 2,
    GGUF_METADATA_VALUE_TYPE_INT16 = 3,
    GGUF_METADATA_VALUE_TYPE_UINT32 = 4,
    GGUF_METADATA_VALUE_TYPE_INT32 = 5,
    GGUF_METADATA_VALUE_TYPE_FLOAT32 = 6,
    GGUF_METADATA_VALUE_TYPE_BOOL = 7,
    GGUF_METADATA_VALUE_TYPE_STRING = 8,
    GGUF_METADATA_VALUE_TYPE_ARRAY = 9,
    GGUF_METADATA_VALUE_TYPE_UINT64 = 10,
    GGUF_METADATA_VALUE_TYPE_INT64 = 11,
    GGUF_METADATA_VALUE_TYPE_FLOAT64 = 12,
};

enum ggml_type : uint32_t {
    GGML_TYPE_F32 = 0,
    GGML_TYPE_F16 = 1,
    GGML_TYPE_Q4_0 = 2,
    GGML_TYPE_Q4_1 = 3,
    GGML_TYPE_Q5_0 = 6,
    GGML_TYPE_Q5_1 = 7,
    GGML_TYPE_Q8_0 = 8,
    GGML_TYPE_Q8_1 = 9,
    GGML_TYPE_Q2_K = 10,
    GGML_TYPE_Q3_K = 11,
    GGML_TYPE_Q4_K = 12,
    GGML_TYPE_Q5_K = 13,
    GGML_TYPE_Q6_K = 14,
    GGML_TYPE_Q8_K = 15,
    GGML_TYPE_IQ2_XXS = 16,
    GGML_TYPE_IQ2_XS = 17,
    GGML_TYPE_IQ3_XXS = 18,
    GGML_TYPE_IQ1_S = 19,
    GGML_TYPE_IQ4_NL = 20,
    GGML_TYPE_IQ3_S = 21,
    GGML_TYPE_IQ2_S = 22,
    GGML_TYPE_IQ4_XS = 23,
    GGML_TYPE_I8 = 24,
    GGML_TYPE_I16 = 25,
    GGML_TYPE_I32 = 26,
    GGML_TYPE_I64 = 27,
    GGML_TYPE_F64 = 28,
    GGML_TYPE_IQ1_M = 29,
    GGML_TYPE_BF16 = 30,
    GGML_TYPE_TQ1_0 = 34,
    GGML_TYPE_TQ2_0 = 35,
    GGML_TYPE_MXFP4 = 39,
    GGML_TYPE_COUNT = 40,
};

std::string gguf_type_to_str(gguf_metadata_value_type type);
std::string ggml_type_to_str(ggml_type type);


struct GGUFMetadataRecord {
    std::string key;
    gguf_metadata_value_type type;
    std::string value_str; 
    bool is_array;         
    uint64_t array_elements_count = 0;

    std::vector<std::string> array_items;
};

struct GGUFTensorRecord {
    std::string name;
    ggml_type type;
    std::vector<uint64_t> shape; 
    uint64_t element_count = 0;
    uint64_t size_bytes = 0;
    
    std::string get_shape_str() const {
        if (shape.empty()) return "[1]";
        std::string s = "[";
        for (size_t i = 0; i < shape.size(); ++i) {
            s += std::to_string(shape[i]);
            if (i < shape.size() - 1) s += " x ";
        }
        s += "]";
        return s;
    }
};

class GGUFReader {
public:
    GGUFReader() = default;
    ~GGUFReader() = default;

    bool skim_file(const std::string& file_path);
    void clear();

    uint32_t get_version() const { return file_version; } 
    const std::vector<GGUFMetadataRecord>& get_metadata() const { return metadata_list; }
    const std::vector<GGUFTensorRecord>& get_tensors() const { return tensor_list; }

private:
    uint32_t file_version = 0; 
    std::vector<GGUFMetadataRecord> metadata_list;
    std::vector<GGUFTensorRecord> tensor_list;
};