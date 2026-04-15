#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace apkx {

// DEX magic numbers
constexpr uint8_t DEX_MAGIC[8] = { 'd', 'e', 'x', '\n', '0', '3', '5', '\0' };

// DEX header structures
struct DexHeader {
    uint8_t magic[8];
    uint32_t checksum;
    uint8_t signature[20];
    uint32_t file_size;
    uint32_t header_size;
    uint32_t endian_tag;
    uint32_t link_size;
    uint32_t link_off;
    uint32_t map_off;
    uint32_t string_ids_size;
    uint32_t string_ids_off;
    uint32_t type_ids_size;
    uint32_t type_ids_off;
    uint32_t proto_ids_size;
    uint32_t proto_ids_off;
    uint32_t field_ids_size;
    uint32_t field_ids_off;
    uint32_t method_ids_size;
    uint32_t method_ids_off;
    uint32_t class_defs_size;
    uint32_t class_defs_off;
    uint32_t data_size;
    uint32_t data_off;
};

struct TypeId {
    uint32_t descriptor_idx;
};

struct ProtoId {
    uint32_t shorty_idx;
    uint32_t return_type_idx;
    uint32_t parameters_off;
};

struct FieldId {
    uint16_t class_idx;
    uint16_t type_idx;
    uint32_t name_idx;
};

struct MethodId {
    uint16_t class_idx;
    uint16_t proto_idx;
    uint32_t name_idx;
};

struct ClassDef {
    uint32_t class_idx;
    uint32_t access_flags;
    uint32_t superclass_idx;
    uint32_t interfaces_off;
    uint32_t source_file_idx;
    uint32_t annotations_off;
    uint32_t class_data_off;
    uint32_t static_values_off;
};

// Method code item (DEX bytecode)
struct MethodCode {
	uint32_t method_idx = 0;
	uint32_t access_flags = 0;
	uint16_t registers_size = 0;
	uint16_t ins_size = 0;
	uint16_t outs_size = 0;
	uint32_t code_size = 0;  // in bytes
	std::vector<uint8_t> bytecode;
	bool valid = false;
};

// Loaded class info
struct LoadedClass {
	std::string name;
	std::string superclass;
	uint32_t access_flags;
	std::map<std::string, MethodCode> methods; // method name -> code
	uint32_t class_data_off = 0;
};

// DEX loader - parses Android DEX files
class DexLoader {
public:
    DexLoader();
    ~DexLoader();

    bool load(const std::string& filepath);
    bool loadFromMemory(std::vector<uint8_t> data);
    
    bool isValid() const { return valid_; }
    
    // Access parsed data
    const std::vector<std::string>& getStrings() const { return strings_; }
    const std::vector<TypeId>& getTypeIds() const { return type_ids_; }
    const std::vector<ProtoId>& getProtoIds() const { return proto_ids_; }
    const std::vector<FieldId>& getFieldIds() const { return field_ids_; }
    const std::vector<MethodId>& getMethodIds() const { return method_ids_; }
    const std::vector<ClassDef>& getClassDefs() const { return class_defs_; }
    const std::map<std::string, LoadedClass>& getClasses() const { return classes_; }
    
    // Lookup helpers
    std::string getTypeName(uint32_t type_idx) const;
    std::string getMethodName(uint32_t method_idx) const;
    std::string getClassName(uint32_t class_idx) const;
    MethodCode getMethodCode(uint32_t method_idx) const;
    MethodCode getMethodCode(const std::string& class_name, const std::string& method_name) const;
    
    // Info
    void dump() const;
    size_t getClassCount() const { return classes_.size(); }
    size_t getMethodCount() const { return method_ids_.size(); }

private:
    bool parseHeader();
    bool parseStrings();
    bool parseTypes();
    bool parseProtos();
    bool parseFields();
    bool parseMethods();
    bool parseClassDefs();
    bool loadClasses();
    bool parseClassData();  // Parse class_data_item for all classes
    
    // LEB128 decoding (DEX variable-length encoding)
    uint32_t readULEB128(const uint8_t*& ptr) const;
    int32_t readSLEB128(const uint8_t*& ptr) const;
    
    // Parse individual class data
    bool parseClassDataItem(uint32_t offset, LoadedClass& cls);
    MethodCode parseCodeItem(uint32_t code_off, uint32_t method_idx);
    
    std::string readString(uint32_t offset) const;
    std::vector<uint16_t> readTypeList(uint32_t offset) const;

    std::vector<uint8_t> data_;
    bool valid_;
    DexHeader header_{};
    uint32_t string_data_base_ = 0;  // Base offset for string_data items
    
    std::vector<std::string> strings_;
    std::vector<uint32_t> string_offsets_;
    std::vector<TypeId> type_ids_;
    std::vector<ProtoId> proto_ids_;
    std::vector<FieldId> field_ids_;
    std::vector<MethodId> method_ids_;
    std::vector<ClassDef> class_defs_;
    std::map<std::string, LoadedClass> classes_;
    
    // Method code lookup by index - populated during class data parsing
    std::map<uint32_t, MethodCode> method_code_by_idx_;
};

} // namespace apkx
