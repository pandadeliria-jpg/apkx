#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace apkx {

// ELF64 constants
constexpr uint32_t ELFMAG0 = 0x7f;
constexpr char ELFMAG1 = 'E';
constexpr char ELFMAG2 = 'L';
constexpr char ELFMAG3 = 'F';
constexpr uint8_t ELFCLASS64 = 2;
constexpr uint8_t ELFDATA2LSB = 1;
constexpr uint16_t EM_AARCH64 = 183;
constexpr uint16_t ET_DYN = 3;

// Section types
constexpr uint32_t SHT_NULL = 0;
constexpr uint32_t SHT_PROGBITS = 1;
constexpr uint32_t SHT_SYMTAB = 2;
constexpr uint32_t SHT_STRTAB = 3;
constexpr uint32_t SHT_RELA = 4;
constexpr uint32_t SHT_HASH = 5;
constexpr uint32_t SHT_DYNAMIC = 6;
constexpr uint32_t SHT_DYNSYM = 11;

// ELF64 headers
struct Elf64_Ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

struct Elf64_Sym {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

struct Elf64_Dyn {
    int64_t d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
};

// Dynamic tags
constexpr int64_t DT_NULL = 0;
constexpr int64_t DT_NEEDED = 1;
constexpr int64_t DT_STRTAB = 5;
constexpr int64_t DT_SYMTAB = 6;
constexpr int64_t DT_STRSZ = 10;
constexpr int64_t DT_SYMENT = 11;

struct Section {
    std::string name;
    uint32_t type;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addralign;
    uint64_t entsize;
    std::vector<uint8_t> data;
};

struct Symbol {
    std::string name;
    uint64_t value;
    uint64_t size;
    uint8_t info;
    uint16_t shndx;
};

// ELF loader for ARM64 Android native libraries
class ElfLoader {
public:
    ElfLoader();
    ~ElfLoader();

    // Load from file or memory
    bool load(const std::string& filepath);
    bool loadFromMemory(std::vector<uint8_t> data);
    
    bool isValid() const { return valid_; }
    bool isArm64() const { return header_.e_machine == EM_AARCH64; }
    
    // Symbol operations
    std::optional<Symbol> findSymbol(const std::string& name) const;
    std::vector<std::string> listExports() const;
    std::vector<std::string> getDependencies() const;
    
    // Section access
    const Section* getSection(const std::string& name) const;
    const Section* getSectionByType(uint32_t type) const;
    
    // Info
    void dump() const;
    const std::string& getFilename() const { return filename_; }
    
    // Check if this is actually a ZIP bootstrap (like libtermux-bootstrap.so)
    bool isZipBootstrap() const;
    
    // ACTUAL EXECUTION - load and run ARM64 code on Apple Silicon
    bool loadExecutable();
    void* getEntryPoint() const;
    void* lookupSymbol(const std::string& name) const;
    
    // Execute loaded code
    int execute(int argc, char** argv);

private:
    bool parseHeader();
    bool parseSections();
    bool parseSymbols();
    bool parseDynamic();
    std::string readString(uint64_t offset) const;

    std::vector<uint8_t> data_;
    std::string filename_;
    bool valid_;
    
    Elf64_Ehdr header_{};
    std::vector<Section> sections_;
    std::vector<Symbol> symbols_;
    std::vector<Symbol> dynamic_symbols_;
    std::map<std::string, size_t> symbol_map_;
    std::map<std::string, size_t> dynamic_symbol_map_;
    std::vector<std::string> dependencies_;
    
    // Dynamic section info
    uint64_t dyn_strtab_offset_ = 0;
    uint64_t dyn_strtab_size_ = 0;
    uint64_t dyn_symtab_offset_ = 0;
    uint64_t dyn_symtab_entsize_ = 0;
};

} // namespace apkx
