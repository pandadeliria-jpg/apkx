#include "apkx/elf_loader.hpp"

#include <fstream>
#include <iostream>
#include <cstring>

namespace apkx {

ElfLoader::ElfLoader() : valid_(false) {
    std::memset(&header_, 0, sizeof(header_));
}

ElfLoader::~ElfLoader() = default;

bool ElfLoader::load(const std::string& filepath) {
    filename_ = filepath;
    
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[!] Failed to open: " << filepath << "\n";
        return false;
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    data_.resize(size);
    if (!file.read(reinterpret_cast<char*>(data_.data()), size)) {
        std::cerr << "[!] Failed to read file\n";
        return false;
    }
    
    return loadFromMemory(std::move(data_));
}

bool ElfLoader::loadFromMemory(std::vector<uint8_t> data) {
    data_ = std::move(data);
    
    if (data_.size() < sizeof(Elf64_Ehdr)) {
        std::cerr << "[!] File too small for ELF header\n";
        return false;
    }
    
    // Check magic
    if (data_[0] != ELFMAG0 || data_[1] != ELFMAG1 || 
        data_[2] != ELFMAG2 || data_[3] != ELFMAG3) {
        return false;  // Not an ELF - might be ZIP/bootstrap
    }
    
    if (!parseHeader()) return false;
    if (!parseSections()) return false;
    if (!parseSymbols()) return false;
    if (!parseDynamic()) return false;
    
    valid_ = true;
    return true;
}

bool ElfLoader::parseHeader() {
    std::memcpy(&header_, data_.data(), sizeof(Elf64_Ehdr));
    
    if (header_.e_ident[4] != ELFCLASS64) {
        std::cerr << "[!] Not 64-bit ELF\n";
        return false;
    }
    
    if (header_.e_ident[5] != ELFDATA2LSB) {
        std::cerr << "[!] Not little-endian\n";
        return false;
    }
    
    if (header_.e_machine != EM_AARCH64) {
        std::cerr << "[!] Not ARM64 (machine=" << header_.e_machine << ")\n";
        // Still valid ELF, just different arch
    }
    
    return true;
}

bool ElfLoader::parseSections() {
    if (header_.e_shoff == 0 || header_.e_shnum == 0) {
        return true;
    }
    
    // Read section header string table header
    Elf64_Shdr shstr_hdr;
    size_t shstr_off = header_.e_shoff + header_.e_shstrndx * sizeof(Elf64_Shdr);
    if (shstr_off + sizeof(Elf64_Shdr) > data_.size()) return false;
    std::memcpy(&shstr_hdr, data_.data() + shstr_off, sizeof(Elf64_Shdr));
    
    for (uint16_t i = 0; i < header_.e_shnum; i++) {
        Elf64_Shdr shdr;
        size_t off = header_.e_shoff + i * sizeof(Elf64_Shdr);
        if (off + sizeof(Elf64_Shdr) > data_.size()) continue;
        std::memcpy(&shdr, data_.data() + off, sizeof(Elf64_Shdr));
        
        Section sec;
        sec.name = readString(shstr_hdr.sh_offset + shdr.sh_name);
        sec.type = shdr.sh_type;
        sec.addr = shdr.sh_addr;
        sec.offset = shdr.sh_offset;
        sec.size = shdr.sh_size;
        sec.link = shdr.sh_link;
        sec.info = shdr.sh_info;
        sec.addralign = shdr.sh_addralign;
        sec.entsize = shdr.sh_entsize;
        
        if (shdr.sh_type != SHT_NULL && shdr.sh_offset + shdr.sh_size <= data_.size()) {
            sec.data.assign(data_.begin() + shdr.sh_offset,
                           data_.begin() + shdr.sh_offset + shdr.sh_size);
        }
        
        sections_.push_back(std::move(sec));
    }
    
    return true;
}

bool ElfLoader::parseSymbols() {
    for (const auto& sec : sections_) {
        if (sec.type == SHT_SYMTAB && sec.link < sections_.size()) {
            const auto& strtab = sections_[sec.link];
            
            size_t count = sec.size / sizeof(Elf64_Sym);
            for (size_t i = 0; i < count; i++) {
                Elf64_Sym sym;
                if (i * sizeof(Elf64_Sym) + sizeof(Elf64_Sym) > sec.data.size()) break;
                std::memcpy(&sym, sec.data.data() + i * sizeof(Elf64_Sym), sizeof(Elf64_Sym));
                
                if (sym.st_name == 0) continue;
                
                Symbol s;
                s.name = readString(strtab.offset + sym.st_name);
                s.value = sym.st_value;
                s.size = sym.st_size;
                s.info = sym.st_info;
                s.shndx = sym.st_shndx;
                
                if (!s.name.empty()) {
                    symbol_map_[s.name] = symbols_.size();
                    symbols_.push_back(std::move(s));
                }
            }
        }
    }
    return true;
}

bool ElfLoader::parseDynamic() {
    const Section* dynamic = getSectionByType(SHT_DYNAMIC);
    if (!dynamic) return true;
    
    // First pass: collect addresses
    size_t count = dynamic->size / sizeof(Elf64_Dyn);
    for (size_t i = 0; i < count; i++) {
        Elf64_Dyn dyn;
        if (i * sizeof(Elf64_Dyn) + sizeof(Elf64_Dyn) > dynamic->data.size()) break;
        std::memcpy(&dyn, dynamic->data.data() + i * sizeof(Elf64_Dyn), sizeof(Elf64_Dyn));
        
        if (dyn.d_tag == DT_NULL) break;
        
        switch (dyn.d_tag) {
            case DT_STRTAB: dyn_strtab_offset_ = dyn.d_un.d_ptr; break;
            case DT_STRSZ: dyn_strtab_size_ = dyn.d_un.d_val; break;
            case DT_SYMTAB: dyn_symtab_offset_ = dyn.d_un.d_ptr; break;
            case DT_SYMENT: dyn_symtab_entsize_ = dyn.d_un.d_val; break;
            default: break;
        }
    }
    
    // Find dynstr section by address
    const Section* dynstr = nullptr;
    for (const auto& sec : sections_) {
        if (sec.addr == dyn_strtab_offset_) {
            dynstr = &sec;
            break;
        }
    }
    
    // Find dynsym section
    const Section* dynsym = getSectionByType(SHT_DYNSYM);
    
    // Parse dynamic symbols
    if (dynsym && dynstr) {
        size_t sym_count = dynsym->size / sizeof(Elf64_Sym);
        for (size_t i = 0; i < sym_count; i++) {
            Elf64_Sym sym;
            if (i * sizeof(Elf64_Sym) + sizeof(Elf64_Sym) > dynsym->data.size()) break;
            std::memcpy(&sym, dynsym->data.data() + i * sizeof(Elf64_Sym), sizeof(Elf64_Sym));
            
            if (sym.st_name == 0) continue;
            
            Symbol s;
            s.name = readString(dynstr->offset + sym.st_name);
            s.value = sym.st_value;
            s.size = sym.st_size;
            s.info = sym.st_info;
            s.shndx = sym.st_shndx;
            
            if (!s.name.empty()) {
                dynamic_symbol_map_[s.name] = dynamic_symbols_.size();
                dynamic_symbols_.push_back(std::move(s));
            }
        }
    }
    
    // Parse dependencies
    if (dynstr) {
        for (size_t i = 0; i < count; i++) {
            Elf64_Dyn dyn;
            if (i * sizeof(Elf64_Dyn) + sizeof(Elf64_Dyn) > dynamic->data.size()) break;
            std::memcpy(&dyn, dynamic->data.data() + i * sizeof(Elf64_Dyn), sizeof(Elf64_Dyn));
            
            if (dyn.d_tag == DT_NEEDED) {
                std::string dep = readString(dynstr->offset + dyn.d_un.d_val);
                if (!dep.empty()) {
                    dependencies_.push_back(dep);
                }
            }
        }
    }
    
    return true;
}

std::string ElfLoader::readString(uint64_t offset) const {
    if (offset >= data_.size()) return "";
    const char* str = reinterpret_cast<const char*>(data_.data() + offset);
    size_t max_len = data_.size() - offset;
    return std::string(str, strnlen(str, max_len));
}

const Section* ElfLoader::getSection(const std::string& name) const {
    for (const auto& sec : sections_) {
        if (sec.name == name) return &sec;
    }
    return nullptr;
}

const Section* ElfLoader::getSectionByType(uint32_t type) const {
    for (const auto& sec : sections_) {
        if (sec.type == type) return &sec;
    }
    return nullptr;
}

std::optional<Symbol> ElfLoader::findSymbol(const std::string& name) const {
    auto it = dynamic_symbol_map_.find(name);
    if (it != dynamic_symbol_map_.end()) {
        return dynamic_symbols_[it->second];
    }
    it = symbol_map_.find(name);
    if (it != symbol_map_.end()) {
        return symbols_[it->second];
    }
    return std::nullopt;
}

std::vector<std::string> ElfLoader::listExports() const {
    std::vector<std::string> exports;
    for (const auto& sym : dynamic_symbols_) {
        uint8_t bind = (sym.info >> 4) & 0xF;
        if ((bind == 1 || bind == 2) && sym.shndx != 0) {
            exports.push_back(sym.name);
        }
    }
    return exports;
}

std::vector<std::string> ElfLoader::getDependencies() const {
    return dependencies_;
}

bool ElfLoader::isZipBootstrap() const {
    // Check for ZIP magic at start
    if (data_.size() >= 4) {
        if (data_[0] == 'P' && data_[1] == 'K' && data_[2] == 0x03 && data_[3] == 0x04) {
            return true;
        }
    }
    return false;
}

void ElfLoader::dump() const {
    std::cout << "\nELF: " << filename_ << "\n";
    std::cout << "  Type: " << header_.e_type << "\n";
    std::cout << "  Machine: " << header_.e_machine << " (ARM64=" << EM_AARCH64 << ")\n";
    std::cout << "  Entry: 0x" << std::hex << header_.e_entry << std::dec << "\n";
    std::cout << "  Sections: " << sections_.size() << "\n";
    
    for (const auto& sec : sections_) {
        if (sec.name == ".text" || sec.name == ".data" || 
            sec.name == ".rodata" || sec.name == ".dynsym") {
            std::cout << "  Section " << sec.name << ": 0x" 
                      << std::hex << sec.addr << std::dec 
                      << " (" << sec.size << " bytes)\n";
        }
    }
    
    auto exports = listExports();
    std::cout << "  Exports: " << exports.size() << "\n";
    for (size_t i = 0; i < std::min(exports.size(), size_t(20)); i++) {
        std::cout << "    " << exports[i] << "\n";
    }
    if (exports.size() > 20) {
        std::cout << "    ... " << (exports.size() - 20) << " more\n";
    }
}

// ACTUAL NATIVE CODE EXECUTION
#include <sys/mman.h>
#include <dlfcn.h>
#include <libkern/OSCacheControl.h>  // sys_icache_invalidate
#include <errno.h>
#include <string.h>

// Loaded executable info
struct LoadedExec {
    void* base_addr = nullptr;
    size_t size = 0;
    std::map<std::string, void*> symbols;
};

static std::map<std::string, LoadedExec> g_loaded_execs;

bool ElfLoader::loadExecutable() {
    if (!valid_ || !isArm64()) {
        std::cerr << "[!] Not valid ARM64 ELF\n";
        return false;
    }
    
    std::cout << "[ELF] Loading executable into memory...\n";
    
    // Find .text section
    const Section* text = getSection(".text");
    if (!text) {
        std::cerr << "[!] No .text section found\n";
        return false;
    }
    
    // macOS: Allocate RW first, then change to RX
    size_t alloc_size = ((text->size + 16383) / 16384) * 16384; // Page align to 16KB
    
    // Allocate as RW first (macOS security requires this)
    void* mem = mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        std::cerr << "[!] Failed to allocate memory\n";
        return false;
    }
    
    // Copy code
    std::memcpy(mem, text->data.data(), text->size);
    
    // Flush instruction cache (ARM64 requirement)
    sys_icache_invalidate(mem, text->size);
    
    // Change to RX (Read + Execute) - no Write for security
    if (mprotect(mem, alloc_size, PROT_READ | PROT_EXEC) != 0) {
        std::cerr << "[!] Failed to set executable permissions: " << strerror(errno) << "\n";
        munmap(mem, alloc_size);
        return false;
    }
    
    // Store loaded info
    LoadedExec exec;
    exec.base_addr = mem;
    exec.size = alloc_size;
    
    // Map symbols to addresses
    for (const auto& sym : dynamic_symbols_) {
        if (sym.shndx != 0) { // Defined symbol
            void* addr = static_cast<char*>(mem) + sym.value;
            exec.symbols[sym.name] = addr;
        }
    }
    
    g_loaded_execs[filename_] = exec;
    
    std::cout << "[ELF] Loaded at " << mem << " (" << text->size << " bytes executable)\n";
    std::cout << "[ELF] " << exec.symbols.size() << " symbols mapped\n";
    
    return true;
}

void* ElfLoader::getEntryPoint() const {
    auto it = g_loaded_execs.find(filename_);
    if (it == g_loaded_execs.end()) return nullptr;
    
    if (header_.e_entry == 0) return nullptr;
    
    // Calculate entry point relative to base
    return static_cast<char*>(it->second.base_addr) + header_.e_entry;
}

void* ElfLoader::lookupSymbol(const std::string& name) const {
    auto it = g_loaded_execs.find(filename_);
    if (it == g_loaded_execs.end()) return nullptr;
    
    auto sym = it->second.symbols.find(name);
    if (sym != it->second.symbols.end()) {
        return sym->second;
    }
    return nullptr;
}

int ElfLoader::execute(int argc, char** argv) {
    if (!loadExecutable()) {
        return -1;
    }
    
    void* entry = getEntryPoint();
    if (!entry) {
        std::cerr << "[!] No entry point found\n";
        return -1;
    }
    
    std::cout << "[ELF] Executing at entry point: " << entry << "\n";
    
    // Cast to function pointer and call
    typedef int (*EntryFunc)(int, char**);
    EntryFunc func = reinterpret_cast<EntryFunc>(entry);
    
    // ACTUALLY EXECUTE THE NATIVE CODE
    return func(argc, argv);
}

} // namespace apkx
