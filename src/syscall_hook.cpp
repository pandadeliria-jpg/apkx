#include "apkx/syscall_hook.hpp"

#include <iostream>
#include <cstring>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

// mach headers for macOS thread state
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/thread_status.h>
#endif

namespace apkx {
namespace runtime {

// SyscallInterposer implementation
SyscallInterposer::SyscallInterposer() = default;
SyscallInterposer::~SyscallInterposer() {
    stopEventLoop();
}

bool SyscallInterposer::initialize() {
    std::cout << "[*] SyscallInterposer initializing...\n";
    
    #ifdef __APPLE__
    if (!initMachPort()) {
        std::cerr << "[!] Failed to initialize Mach port\n";
        return false;
    }
    #endif
    
    active_ = true;
    std::cout << "[+] Syscall interposer ready\n";
    return true;
}

void SyscallInterposer::registerHandler(uint64_t syscall_num, SyscallHandler handler) {
    handlers_[syscall_num] = handler;
}

void SyscallInterposer::registerDefaultHandler(SyscallHandler handler) {
    default_handler_ = handler;
}

bool SyscallInterposer::spawnAndTrace(const std::string& executable,
                                        const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "[!] fork() failed: " << strerror(errno) << "\n";
        return false;
    }
    
    if (pid == 0) {
        // Child process
        // Enable tracing
        if (ptrace(PT_TRACE_ME, 0, nullptr, 0) == -1) {
            std::cerr << "[!] PT_TRACE_ME failed: " << strerror(errno) << "\n";
            _exit(1);
        }
        
        // Convert args to char* array
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(executable.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        // Execute
        execvp(executable.c_str(), argv.data());
        std::cerr << "[!] execvp failed: " << strerror(errno) << "\n";
        _exit(1);
    }
    
    // Parent process
    traced_pid_ = pid;
    std::cout << "[*] Tracing PID " << pid << "\n";
    
    // Wait for initial stop
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        std::cerr << "[!] waitpid failed: " << strerror(errno) << "\n";
        return false;
    }
    
    if (!WIFSTOPPED(status)) {
        std::cerr << "[!] Child not stopped\n";
        return false;
    }
    
    std::cout << "[+] Child stopped, ready to trace\n";
    return true;
}

bool SyscallInterposer::attachToProcess(pid_t pid) {
    traced_pid_ = pid;
    
    if (ptrace(PT_ATTACH, pid, nullptr, 0) == -1) {
        std::cerr << "[!] PT_ATTACH failed: " << strerror(errno) << "\n";
        return false;
    }
    
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        std::cerr << "[!] waitpid failed\n";
        return false;
    }
    
    std::cout << "[+] Attached to PID " << pid << "\n";
    return true;
}

void SyscallInterposer::runEventLoop() {
    if (!active_ || traced_pid_ == -1) {
        std::cerr << "[!] Not initialized or no process to trace\n";
        return;
    }
    
    running_ = true;
    std::cout << "[*] Starting syscall event loop...\n";
    std::cout << "[!] Note: macOS ptrace is limited. Full syscall tracing requires kernel extension or different approach.\n";
    
    // On macOS, we use PT_CONTINUE and step through instructions
    // looking for SVC instructions (ARM64 syscalls)
    while (running_) {
        // Continue execution
        if (ptrace(PT_CONTINUE, traced_pid_, reinterpret_cast<caddr_t>(1), 0) == -1) {
            if (errno == ESRCH) {
                std::cout << "[*] Process exited\n";
                break;
            }
            std::cerr << "[!] PT_CONTINUE failed: " << strerror(errno) << "\n";
            break;
        }
        
        // Wait for stop
        if (!waitForSyscall(traced_pid_)) {
            break;
        }
        
        // On macOS with ARM64, we can check if we're at a syscall
        // by examining the instruction at PC
        ARM64Registers regs;
        if (getRegisters(traced_pid_, regs)) {
            // Read instruction at PC
            uint32_t insn = 0;
            if (readMemory(traced_pid_, regs.pc, &insn, sizeof(insn))) {
                // Check for SVC instruction (0xd4000001)
                if ((insn & 0xFFE0001F) == 0xd4000001) {
                    // Found syscall
                    handleSyscallEntry(traced_pid_);
                }
            }
        }
    }
    
    running_ = false;
}

void SyscallInterposer::stopEventLoop() {
    running_ = false;
}

bool SyscallInterposer::waitForSyscall(pid_t pid) {
    int status;
    pid_t ret = waitpid(pid, &status, 0);
    
    if (ret == -1) {
        std::cerr << "[!] waitpid failed: " << strerror(errno) << "\n";
        return false;
    }
    
    if (WIFEXITED(status)) {
        std::cout << "[*] Process exited with code " << WEXITSTATUS(status) << "\n";
        return false;
    }
    
    if (WIFSIGNALED(status)) {
        std::cout << "[*] Process terminated by signal " << WTERMSIG(status) << "\n";
        return false;
    }
    
    if (!WIFSTOPPED(status)) {
        std::cerr << "[!] Unexpected process state\n";
        return false;
    }
    
    int sig = WSTOPSIG(status);
    if (sig != SIGTRAP) {
        // Forward other signals
        if (ptrace(PT_CONTINUE, pid, reinterpret_cast<caddr_t>(1), sig) == -1) {
            std::cerr << "[!] PT_CONTINUE failed\n";
            return false;
        }
        return waitForSyscall(pid);
    }
    
    return true;
}

bool SyscallInterposer::handleSyscallEntry(pid_t pid) {
    ARM64Registers regs;
    if (!getRegisters(pid, regs)) {
        return false;
    }
    
    uint64_t syscall_num = regs.getSyscallNum();
    uint64_t arg0 = regs.getArg(0);
    uint64_t arg1 = regs.getArg(1);
    uint64_t arg2 = regs.getArg(2);
    uint64_t arg3 = regs.getArg(3);
    uint64_t arg4 = regs.getArg(4);
    uint64_t arg5 = regs.getArg(5);
    
    std::cout << "[SYSCALL] #" << syscall_num << " (" << arg0 << ", " << arg1 << ", ...)\n";
    
    // Check if we have a handler for this syscall
    auto it = handlers_.find(syscall_num);
    if (it != handlers_.end()) {
        uint64_t result = 0;
        if (it->second(syscall_num, arg0, arg1, arg2, arg3, arg4, arg5, result)) {
            // Handler processed the syscall, inject result
            regs.x[0] = result;
            setRegisters(pid, regs);
            
            // Skip actual syscall by advancing PC past it
            regs.pc += 4;  // ARM64 syscall instruction is 4 bytes
            setRegisters(pid, regs);
        }
    } else if (default_handler_) {
        uint64_t result = 0;
        default_handler_(syscall_num, arg0, arg1, arg2, arg3, arg4, arg5, result);
    }
    
    return true;
}

bool SyscallInterposer::handleSyscallExit(pid_t pid) {
    // Read syscall result
    ARM64Registers regs;
    if (!getRegisters(pid, regs)) {
        return false;
    }
    
    uint64_t result = regs.x[0];
    // std::cout << "[SYSCALL] Returned: " << result << "\n";
    
    return true;
}

bool SyscallInterposer::getRegisters(pid_t pid, ARM64Registers& regs) {
    #ifdef __APPLE__
    // Use thread_get_state on macOS
    // This requires task port access
    
    // Simplified: use ptrace PT_GETREGS if available
    // For full ARM64 support on macOS, we need the ARM thread state flavor
    
    // Zero out for now (would need mach thread state)
    std::memset(&regs, 0, sizeof(regs));
    
    // Alternative: use ptrace with PT_READ_I to read from PC
    errno = 0;
    long val = ptrace(PT_READ_I, pid, nullptr, 0);
    if (errno != 0) {
        std::cerr << "[!] PT_READ_I failed: " << strerror(errno) << "\n";
        return false;
    }
    
    return true;
    #else
    // Linux: use PTRACE_GETREGS
    std::memset(&regs, 0, sizeof(regs));
    return false;
    #endif
}

bool SyscallInterposer::setRegisters(pid_t pid, const ARM64Registers& regs) {
    // macOS implementation would use thread_set_state
    // For now, this is a stub
    return true;
}

bool SyscallInterposer::readMemory(pid_t pid, uint64_t addr, void* buf, size_t size) {
    #ifdef PT_IO
    // Use ptrace PT_IO for memory reads
    for (size_t i = 0; i < size; i += sizeof(long)) {
        errno = 0;
        long val = ptrace(PT_READ_D, pid, reinterpret_cast<void*>(addr + i), 0);
        if (errno != 0) {
            std::cerr << "[!] PT_READ_D failed at " << (addr + i) << "\n";
            return false;
        }
        std::memcpy(static_cast<uint8_t*>(buf) + i, &val, sizeof(long));
    }
    return true;
    #else
    return false;
    #endif
}

bool SyscallInterposer::writeMemory(pid_t pid, uint64_t addr, const void* buf, size_t size) {
    #ifdef PT_IO
    const uint8_t* src = static_cast<const uint8_t*>(buf);
    for (size_t i = 0; i < size; i += sizeof(long)) {
        long val = 0;
        std::memcpy(&val, src + i, sizeof(long));
        if (ptrace(PT_WRITE_D, pid, reinterpret_cast<caddr_t>(addr + i), val) == -1) {
            return false;
        }
    }
    return true;
    #else
    return false;
    #endif
}

bool SyscallInterposer::stepInstruction(pid_t pid) {
    if (ptrace(PT_STEP, pid, reinterpret_cast<caddr_t>(1), 0) == -1) {
        return false;
    }
    
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return false;
    }
    
    return WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP;
}

bool SyscallInterposer::continueExecution(pid_t pid) {
    return ptrace(PT_CONTINUE, pid, reinterpret_cast<caddr_t>(1), 0) != -1;
}

#ifdef __APPLE__
bool SyscallInterposer::initMachPort() {
    // Initialize Mach port for task control
    // This is needed for thread_get_state/set_state on macOS
    mach_port_t self = mach_task_self();
    // Store for later use
    mach_port_ = reinterpret_cast<void*>(static_cast<uintptr_t>(self));
    return true;
}
#endif

// NativeLoader implementation
NativeLoader::NativeLoader() = default;
NativeLoader::~NativeLoader() = default;

bool NativeLoader::loadBinary(const std::string& path) {
    std::cout << "[*] Loading native binary: " << path << "\n";
    
    // Read file
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        std::cerr << "[!] Failed to open: " << path << "\n";
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    binary_data_.resize(size);
    if (fread(binary_data_.data(), 1, size, f) != size) {
        std::cerr << "[!] Failed to read binary\n";
        fclose(f);
        return false;
    }
    fclose(f);
    
    std::cout << "    Loaded " << size << " bytes\n";
    return true;
}

bool NativeLoader::patchSyscalls() {
    std::cout << "[*] Patching syscalls...\n";
    
    // Find SVC #0x80 instructions (ARM64 syscall)
    // ARM64 encoding: 0xd4000001 | (imm << 5)
    // SVC instruction: 0xd4000001
    
    size_t patched = 0;
    for (size_t i = 0; i < binary_data_.size() - 4; i += 4) {
        uint32_t insn = *reinterpret_cast<uint32_t*>(&binary_data_[i]);
        
        // Check for SVC instruction
        if ((insn & 0xFFE0001F) == 0xd4000001) {
            // Found syscall - replace with breakpoint/trap
            // For now, replace with NOP
            uint32_t nop = 0xd503201f;  // ARM64 NOP
            *reinterpret_cast<uint32_t*>(&binary_data_[i]) = nop;
            patched++;
        }
    }
    
    std::cout << "    Patched " << patched << " syscalls\n";
    return true;
}

bool NativeLoader::relocate() {
    std::cout << "[*] Relocating binary...\n";
    // Parse ELF and relocate sections
    // This is complex - for now, assume position-independent code
    return true;
}

int NativeLoader::execute(int argc, char** argv) {
    std::cout << "[!] Direct ARM64 execution not yet implemented on macOS\n";
    std::cout << "    Requires Rosetta 2 extension or dynamic translation\n";
    return -1;
}

uint32_t NativeLoader::encodeSvc(uint16_t imm) {
    // ARM64: SVC #imm
    // Encoding: 1101 0100 0000 0000 0000 0000 000 imm[15:0]
    return 0xd4000001 | (static_cast<uint32_t>(imm) << 5);
}

uint32_t NativeLoader::encodeBranch(uint64_t from, uint64_t to) {
    // ARM64 unconditional branch
    // Simplified: just return B instruction encoding
    int64_t offset = static_cast<int64_t>(to - from);
    offset >>= 2;  // Divide by 4 (instruction size)
    
    // B instruction: 0001 01ii iiii iiii iiii iiii iiii iiii
    uint32_t insn = 0x14000000 | (offset & 0x03FFFFFF);
    return insn;
}

uint32_t NativeLoader::encodeNop() {
    return 0xd503201f;  // NOP
}

// NativeExecutor implementation
NativeExecutor::NativeExecutor() = default;
NativeExecutor::~NativeExecutor() = default;

bool NativeExecutor::initialize() {
    std::cout << "[*] NativeExecutor initializing...\n";
    
    interposer_ = std::make_unique<SyscallInterposer>();
    if (!interposer_->initialize()) {
        return false;
    }
    
    loader_ = std::make_unique<NativeLoader>();
    
    initialized_ = true;
    std::cout << "[+] NativeExecutor ready\n";
    return true;
}

int NativeExecutor::executeLibrary(const std::string& lib_path,
                                    const std::string& entry_symbol,
                                    void* arg) {
    if (!initialized_) {
        std::cerr << "[!] Not initialized\n";
        return -1;
    }
    
    std::cout << "[*] Executing library: " << lib_path << "::" << entry_symbol << "\n";
    
    if (!loader_->loadBinary(lib_path)) {
        return -1;
    }
    
    if (!loader_->patchSyscalls()) {
        return -1;
    }
    
    // TODO: Map into memory, resolve symbols, execute
    std::cout << "[!] Library execution not yet implemented\n";
    
    return -1;
}

int NativeExecutor::executeBinary(const std::string& binary_path,
                                  const std::vector<std::string>& args) {
    if (!initialized_) {
        return -1;
    }
    
    std::cout << "[*] Executing binary: " << binary_path << "\n";
    
    // Spawn and trace
    if (!interposer_->spawnAndTrace(binary_path, args)) {
        return -1;
    }
    
    // Run event loop
    interposer_->runEventLoop();
    
    return 0;
}

void* NativeExecutor::executeJNIMethod(const std::string& class_name,
                                      const std::string& method_name,
                                      const std::string& signature,
                                      void* obj, void** args) {
    std::cout << "[*] JNI: " << class_name << "." << method_name << signature << "\n";
    
    // TODO: Look up in loaded libraries, execute
    
    return nullptr;
}

} // namespace runtime
} // namespace apkx
