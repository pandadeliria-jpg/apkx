#include "apkx/dex_loader.hpp"

#include <fstream>
#include <iostream>
#include <cstring>

namespace apkx {

DexLoader::DexLoader() : valid_(false) {
    std::memset(&header_, 0, sizeof(header_));
}

DexLoader::~DexLoader() = default;

bool DexLoader::load(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[!] Failed to open DEX: " << filepath << "\n";
        return false;
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "[!] Failed to read DEX file\n";
        return false;
    }
    
    return loadFromMemory(std::move(data));
}

bool DexLoader::loadFromMemory(std::vector<uint8_t> data) {
    data_ = std::move(data);
    
    if (data_.size() < sizeof(DexHeader)) {
        std::cerr << "[!] File too small for DEX header\n";
        return false;
    }
    
    // Check magic
    if (std::memcmp(data_.data(), DEX_MAGIC, 4) != 0) {
        std::cerr << "[!] Not a valid DEX file (bad magic)\n";
        return false;
    }
    
    if (!parseHeader()) return false;
    if (!parseStrings()) return false;
    if (!parseTypes()) return false;
    if (!parseProtos()) return false;
    if (!parseFields()) return false;
    if (!parseMethods()) return false;
    if (!parseClassDefs()) return false;
    if (!loadClasses()) return false;
    if (!parseClassData()) return false;
    
    valid_ = true;
    return true;
}

bool DexLoader::parseHeader() {
    std::memcpy(&header_, data_.data(), sizeof(DexHeader));
    
    if (header_.endian_tag != 0x12345678) {
        std::cerr << "[!] Unexpected endian tag: " << std::hex << header_.endian_tag << std::dec << "\n";
    }
    
    return true;
}

bool DexLoader::parseStrings() {
    if (header_.string_ids_size == 0) return true;
    
    // Read string_id_list - contains absolute offsets to string_data items
    string_offsets_.resize(header_.string_ids_size);
    for (uint32_t i = 0; i < header_.string_ids_size; i++) {
        size_t off = header_.string_ids_off + i * sizeof(uint32_t);
        if (off + sizeof(uint32_t) > data_.size()) break;
        uint32_t offset = 0;
        std::memcpy(&offset, data_.data() + off, sizeof(uint32_t));
        string_offsets_[i] = offset;
    }
    
    // Find string_data base from DEX map (where actual string content lives)
    string_data_base_ = 0;
    if (header_.map_off > 0 && header_.map_off < data_.size()) {
        const uint8_t* map_p = data_.data() + header_.map_off;
        uint32_t map_size = 0;
        std::memcpy(&map_size, map_p, 4);
        for (uint32_t i = 0; i < map_size; i++) {
            const uint8_t* entry = map_p + 4 + i * 12;
            uint16_t type = 0;
            std::memcpy(&type, entry, 2);
            uint32_t offset = 0;
            std::memcpy(&offset, entry + 8, 4);
            if (type == 0x2002) {  // STRING_DATA_ITEM
                string_data_base_ = offset;
                break;
            }
        }
    }
    
    // Read all strings using readString
    strings_.resize(header_.string_ids_size);
    for (uint32_t i = 0; i < header_.string_ids_size; i++) {
        strings_[i] = readString(string_offsets_[i]);
    }
    
    return true;
}

bool DexLoader::parseTypes() {
    if (header_.type_ids_size == 0) return true;
    
    type_ids_.resize(header_.type_ids_size);
    for (uint32_t i = 0; i < header_.type_ids_size; i++) {
        size_t off = header_.type_ids_off + i * sizeof(uint32_t);
        if (off + sizeof(uint32_t) > data_.size()) break;
        std::memcpy(&type_ids_[i].descriptor_idx, data_.data() + off, sizeof(uint32_t));
    }
    
    return true;
}

bool DexLoader::parseProtos() {
    if (header_.proto_ids_size == 0) return true;
    
    proto_ids_.resize(header_.proto_ids_size);
    for (uint32_t i = 0; i < header_.proto_ids_size; i++) {
        size_t off = header_.proto_ids_off + i * (3 * sizeof(uint32_t));
        if (off + 3 * sizeof(uint32_t) > data_.size()) break;
        std::memcpy(&proto_ids_[i], data_.data() + off, 3 * sizeof(uint32_t));
    }
    return true;
}

bool DexLoader::parseFields() {
    if (header_.field_ids_size == 0) return true;
    
    field_ids_.resize(header_.field_ids_size);
    for (uint32_t i = 0; i < header_.field_ids_size; i++) {
        size_t off = header_.field_ids_off + i * sizeof(FieldId);
        if (off + sizeof(FieldId) > data_.size()) break;
        std::memcpy(&field_ids_[i], data_.data() + off, sizeof(FieldId));
    }
    return true;
}

bool DexLoader::parseMethods() {
    if (header_.method_ids_size == 0) return true;
    
    method_ids_.resize(header_.method_ids_size);
    for (uint32_t i = 0; i < header_.method_ids_size; i++) {
        size_t off = header_.method_ids_off + i * sizeof(MethodId);
        if (off + sizeof(MethodId) > data_.size()) break;
        std::memcpy(&method_ids_[i], data_.data() + off, sizeof(MethodId));
    }
    return true;
}

bool DexLoader::parseClassDefs() {
    if (header_.class_defs_size == 0) return true;
    
    class_defs_.resize(header_.class_defs_size);
    for (uint32_t i = 0; i < header_.class_defs_size; i++) {
        size_t off = header_.class_defs_off + i * sizeof(ClassDef);
        if (off + sizeof(ClassDef) > data_.size()) break;
        std::memcpy(&class_defs_[i], data_.data() + off, sizeof(ClassDef));
    }
    return true;
}

bool DexLoader::loadClasses() {
    for (const auto& def : class_defs_) {
        if (def.class_idx >= type_ids_.size()) continue;
        
        LoadedClass cls;
        // class_idx is an INDEX into type_ids array
        uint32_t descriptor_str_idx = type_ids_[def.class_idx].descriptor_idx;
        
        // Now use that string index to get the actual string
        if (descriptor_str_idx < strings_.size()) {
            cls.name = strings_[descriptor_str_idx];
        } else {
            cls.name = "<invalid_type>";
        }
        cls.access_flags = def.access_flags;
        
        // Same for superclass
        if (def.superclass_idx != 0xFFFFFFFF && def.superclass_idx < type_ids_.size()) {
            uint32_t super_str_idx = type_ids_[def.superclass_idx].descriptor_idx;
            if (super_str_idx < strings_.size()) {
                cls.superclass = strings_[super_str_idx];
            }
        }
        
        if (!cls.name.empty() && cls.name != "<invalid_type>") {
            classes_[cls.name] = std::move(cls);
        }
    }
    return true;
}

std::string DexLoader::readString(uint32_t offset) const {
    // string_offsets are ABSOLUTE file offsets in standard DEX
    uint32_t absolute_offset = offset;
    
    // DEX spec says string_id_item contains absolute file offset
    // But some minimal DEX files store relative offsets
    
    // If absolute offset is too small, try relative to data section
    if (absolute_offset < header_.data_off || absolute_offset >= data_.size()) {
        if (header_.data_off > 0 && offset < data_.size()) {
            absolute_offset = header_.data_off + offset;
        }
    }
    
    // Final validation 
    if (absolute_offset >= data_.size() || absolute_offset < 0x70) {
        // Strings must be in data section after header (0x70)
        return "";
    }
    
    // DEX strings: ULEB128 length, then MUTF-8 content (includes null in length)
    const uint8_t* p = data_.data() + absolute_offset;
    uint32_t len = 0;
    uint32_t shift = 0;
    uint32_t i = 0;
    
    // Decode ULEB128
    while (i < 5 && absolute_offset + i < data_.size()) {
        uint8_t b = p[i];
        len |= (b & 0x7F) << shift;
        i++;
        if ((b & 0x80) == 0) break;
        shift += 7;
    }
    
    // Actual string length is len-1 (subtract null terminator)
    if (len == 0 || absolute_offset + i >= data_.size()) return "";
    uint32_t actual_len = (len > 0) ? len - 1 : 0;
    if (actual_len > data_.size() - absolute_offset - i) actual_len = data_.size() - absolute_offset - i;
    
    std::string result;
    result.reserve(actual_len);
    for (uint32_t j = 0; j < actual_len; j++) {
        result += static_cast<char>(p[i + j]);
    }
    return result;
}

std::vector<uint16_t> DexLoader::readTypeList(uint32_t offset) const {
    std::vector<uint16_t> result;
    if (offset == 0 || offset >= data_.size()) return result;
    
    uint32_t size;
    std::memcpy(&size, data_.data() + offset, sizeof(uint32_t));
    
    if (offset + 4 + size * 2 > data_.size()) return result;
    
    result.resize(size);
    for (uint32_t i = 0; i < size; i++) {
        std::memcpy(&result[i], data_.data() + offset + 4 + i * 2, sizeof(uint16_t));
    }
    return result;
}

std::string DexLoader::getTypeName(uint32_t type_idx) const {
    if (type_idx >= type_ids_.size()) return "<invalid_type>";
    uint32_t str_idx = type_ids_[type_idx].descriptor_idx;
    if (str_idx >= strings_.size()) return "<invalid_string>";
    return strings_[str_idx];
}

std::string DexLoader::getMethodName(uint32_t method_idx) const {
    if (method_idx >= method_ids_.size()) return "<invalid_method>";
    uint32_t str_idx = method_ids_[method_idx].name_idx;
    if (str_idx >= strings_.size()) return "<invalid_string>";
    return strings_[str_idx];
}

std::string DexLoader::getClassName(uint32_t class_idx) const {
    if (class_idx >= type_ids_.size()) return "<invalid_class>";
    uint32_t str_idx = type_ids_[class_idx].descriptor_idx;
    if (str_idx >= strings_.size()) return "<invalid_string>";
    return strings_[str_idx];
}

void DexLoader::dump() const {
    std::cout << "\nDEX File:\n";
    std::cout << "  File size: " << header_.file_size << " bytes\n";
    std::cout << "  Strings: " << strings_.size() << "\n";
    std::cout << "  Types: " << type_ids_.size() << "\n";
    std::cout << "  Protos: " << proto_ids_.size() << "\n";
    std::cout << "  Fields: " << field_ids_.size() << "\n";
    std::cout << "  Methods: " << method_ids_.size() << "\n";
    std::cout << "  Classes: " << classes_.size() << "\n";
    std::cout << "  Methods with code: " << method_code_by_idx_.size() << "\n";
    
    std::cout << "\n  Classes (first 10):\n";
    int count = 0;
    for (const auto& [name, cls] : classes_) {
        std::cout << "    " << name;
        if (!cls.superclass.empty()) {
            std::cout << " extends " << cls.superclass;
        }
        std::cout << " (" << cls.methods.size() << " methods)\n";
        if (++count >= 10) {
            std::cout << "    ... and " << (classes_.size() - 10) << " more\n";
            break;
        }
    }
}

// LEB128 decoding (DEX variable-length encoding)
uint32_t DexLoader::readULEB128(const uint8_t*& ptr) const {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    do {
        byte = *ptr++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    return result;
}

int32_t DexLoader::readSLEB128(const uint8_t*& ptr) const {
    int32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    do {
        byte = *ptr++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    
    // Sign extend
    if (shift < 32 && (byte & 0x40)) {
        result |= (~0) << shift;
    }
    return result;
}

// Parse code_item structure
MethodCode DexLoader::parseCodeItem(uint32_t code_off, uint32_t method_idx) {
    MethodCode code;
    code.method_idx = method_idx;
    
    if (code_off == 0 || code_off >= data_.size()) {
        return code;  // Abstract or native method
    }
    
    const uint8_t* p = data_.data() + code_off;
    
    // code_item structure:
    // uint16_t registers_size
    // uint16_t ins_size
    // uint16_t outs_size
    // uint16_t tries_size
    // uint32_t debug_info_off
    // uint32_t insns_size (in 16-bit units)
    // uint16_t[insns_size] insns (the bytecode!)
    
    if (code_off + 12 > data_.size()) return code;
    
    std::memcpy(&code.registers_size, p, 2);
    std::memcpy(&code.ins_size, p + 2, 2);
    std::memcpy(&code.outs_size, p + 4, 2);
    
    uint16_t tries_size;
    std::memcpy(&tries_size, p + 6, 2);
    
    uint32_t debug_info_off;
    std::memcpy(&debug_info_off, p + 8, 4);
    
    uint32_t insns_size;  // in 16-bit units
    std::memcpy(&insns_size, p + 12, 4);
    
    code.code_size = insns_size * 2;  // Convert to bytes
    
    // Read the actual bytecode
    if (code_off + 16 + code.code_size > data_.size()) {
        return code;  // Truncated
    }
    
    code.bytecode.resize(code.code_size);
    std::memcpy(code.bytecode.data(), p + 16, code.code_size);
    code.valid = true;
    
    return code;
}

// Parse class_data_item for a single class
bool DexLoader::parseClassDataItem(uint32_t offset, LoadedClass& cls) {
    if (offset == 0 || offset >= data_.size()) return false;
    
    const uint8_t* p = data_.data() + offset;
    const uint8_t* end = data_.data() + data_.size();
    
    // Read header counts
    uint32_t static_fields_size = readULEB128(p);
    uint32_t instance_fields_size = readULEB128(p);
    uint32_t direct_methods_size = readULEB128(p);
    uint32_t virtual_methods_size = readULEB128(p);
    
    // Skip static fields (encoded_field)
    for (uint32_t i = 0; i < static_fields_size && p < end; i++) {
        readULEB128(p);  // field_idx_diff
        readULEB128(p);  // access_flags
    }
    
    // Skip instance fields (encoded_field)
    for (uint32_t i = 0; i < instance_fields_size && p < end; i++) {
        readULEB128(p);  // field_idx_diff
        readULEB128(p);  // access_flags
    }
    
    // Parse direct methods (encoded_method)
    uint32_t prev_method_idx = 0;
    for (uint32_t i = 0; i < direct_methods_size && p < end; i++) {
        uint32_t method_idx_diff = readULEB128(p);
        uint32_t access_flags = readULEB128(p);
        uint32_t code_off = readULEB128(p);
        
        uint32_t method_idx = prev_method_idx + method_idx_diff;
        prev_method_idx = method_idx;
        
        if (method_idx < method_ids_.size()) {
            std::string method_name = getMethodName(method_idx);
            MethodCode code = parseCodeItem(code_off, method_idx);
            code.access_flags = access_flags;
            
            if (code.valid) {
                cls.methods[method_name] = code;
                method_code_by_idx_[method_idx] = code;
            }
        }
    }
    
    // Parse virtual methods (encoded_method)
    prev_method_idx = 0;
    for (uint32_t i = 0; i < virtual_methods_size && p < end; i++) {
        uint32_t method_idx_diff = readULEB128(p);
        uint32_t access_flags = readULEB128(p);
        uint32_t code_off = readULEB128(p);
        
        uint32_t method_idx = prev_method_idx + method_idx_diff;
        prev_method_idx = method_idx;
        
        if (method_idx < method_ids_.size()) {
            std::string method_name = getMethodName(method_idx);
            MethodCode code = parseCodeItem(code_off, method_idx);
            code.access_flags = access_flags;
            
            if (code.valid) {
                cls.methods[method_name] = code;
                method_code_by_idx_[method_idx] = code;
            }
        }
    }
    
    return true;
}

// Parse class data for all classes
bool DexLoader::parseClassData() {
    for (auto& [class_name, cls] : classes_) {
        // Find ClassDef for this class
        for (const auto& def : class_defs_) {
            if (def.class_idx < type_ids_.size()) {
                std::string name = getTypeName(def.class_idx);
                if (name == class_name && def.class_data_off != 0) {
                    cls.class_data_off = def.class_data_off;
                    parseClassDataItem(def.class_data_off, cls);
                    break;
                }
            }
        }
    }
    return true;
}

// Get method code by index
MethodCode DexLoader::getMethodCode(uint32_t method_idx) const {
    auto it = method_code_by_idx_.find(method_idx);
    if (it != method_code_by_idx_.end()) {
        return it->second;
    }
    return MethodCode();  // Return empty/invalid
}

// Get method code by class and method name
MethodCode DexLoader::getMethodCode(const std::string& class_name, const std::string& method_name) const {
    auto it = classes_.find(class_name);
    if (it != classes_.end()) {
        auto mit = it->second.methods.find(method_name);
        if (mit != it->second.methods.end()) {
            return mit->second;
        }
    }
    return MethodCode();  // Return empty/invalid
}

} // namespace apkx
