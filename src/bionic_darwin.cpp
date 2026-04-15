#include "apkx/bionic_darwin.hpp"

#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <pthread.h>

namespace apkx {
namespace runtime {

// Global syscall translator
static std::unique_ptr<BionicDarwinTranslator> g_translator;

BionicDarwinTranslator::BionicDarwinTranslator() {
    initIoctlMap();
}

BionicDarwinTranslator::~BionicDarwinTranslator() = default;

bool BionicDarwinTranslator::initialize(
    const std::string& app_data_dir,
    const std::string& app_cache_dir) {
    
    std::cout << "[*] BionicDarwinTranslator initializing...\n";
    
    // Set up path redirections
    path_redirect_.setAppDataPath("/data/data/com.app.package", app_data_dir);
    path_redirect_.setAppDataPath("/data/user/0/com.app.package", app_data_dir);
    path_redirect_.setAppDataPath("/sdcard", app_data_dir + "/sdcard");
    
    // Set up Android properties
    properties_["ro.product.model"] = "MacBookPro";
    properties_["ro.product.brand"] = "Apple";
    properties_["ro.build.version.sdk"] = "33";
    properties_["ro.build.version.release"] = "13";
    properties_["ro.arch"] = "arm64";
    
    initialized_ = true;
    std::cout << "    Path redirection: OK\n";
    return true;
}

SyscallResult BionicDarwinTranslator::translate(
    uint64_t syscall_number,
    uint64_t arg0, uint64_t arg1, uint64_t arg2,
    uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    
    if (!initialized_) {
        return SyscallResult(-1);
    }
    
    switch (syscall_number) {
        case Syscall::OPENAT:
            return sys_openat(static_cast<int>(arg0),
                          reinterpret_cast<const char*>(arg1),
                          static_cast<int>(arg2), 
                          static_cast<int>(arg3));
        
        case Syscall::READ:
            return sys_read(static_cast<int>(arg0),
                          reinterpret_cast<void*>(arg1),
                          static_cast<size_t>(arg2));
        
        case Syscall::WRITE:
            return sys_write(static_cast<int>(arg0),
                           reinterpret_cast<const void*>(arg1),
                           static_cast<size_t>(arg2));
        
        case Syscall::CLOSE:
            return sys_close(static_cast<int>(arg0));
        
        case Syscall::LSEEK:
            return sys_lseek(static_cast<int>(arg0),
                           static_cast<off_t>(arg1),
                           static_cast<int>(arg2));
        
        case Syscall::MMAP:
            return sys_mmap(reinterpret_cast<void*>(arg0),
                          static_cast<size_t>(arg1),
                          static_cast<int>(arg2),
                          static_cast<int>(arg3),
                          static_cast<int>(arg4),
                          static_cast<off_t>(arg5));
        
        case Syscall::MUNMAP:
            return sys_munmap(reinterpret_cast<void*>(arg0),
                            static_cast<size_t>(arg1));
        
        case Syscall::GETPID:
            return sys_getpid();
        
        case Syscall::GETTID:
            return sys_gettid();
        
        case Syscall::EXIT:
            return sys_exit(static_cast<int>(arg0));
        
        case Syscall::GETTIME: {
            struct timespec ts;
            auto result = sys_gettime(static_cast<int>(arg0), &ts);
            if (result.success && arg1) {
                std::memcpy(reinterpret_cast<void*>(arg1), &ts, sizeof(ts));
            }
            return result;
        }
        
        default:
            std::cerr << "[!] Unimplemented syscall: " << syscall_number << "\n";
            return SyscallResult(-ENOSYS);
    }
}

SyscallResult BionicDarwinTranslator::sys_openat(
    int dirfd, const std::string& pathname, int flags, int mode) {
    
    // Redirect Android path to host path
    std::string host_path = path_redirect_.redirect(pathname);
    
    // Translate Android O_ flags to macOS (mostly compatible)
    int host_flags = flags & 0xFFFF;
    
    int fd = ::open(host_path.c_str(), host_flags, mode);
    if (fd < 0) {
        return SyscallResult(-errno);
    }
    
    // Map to Android fd space
    int android_fd = fd_table_.allocate();
    fd_table_.set(android_fd, fd);
    
    return SyscallResult(android_fd);
}

SyscallResult BionicDarwinTranslator::sys_read(int fd, void* buf, size_t count) {
    int host_fd = fd_table_.get(fd);
    if (host_fd < 0) {
        return SyscallResult(-EBADF);
    }
    
    ssize_t n = ::read(host_fd, buf, count);
    if (n < 0) {
        return SyscallResult(-errno);
    }
    return SyscallResult(n);
}

SyscallResult BionicDarwinTranslator::sys_write(int fd, const void* buf, 
                                                   size_t count) {
    int host_fd = fd_table_.get(fd);
    if (host_fd < 0) {
        return SyscallResult(-EBADF);
    }
    
    ssize_t n = ::write(host_fd, buf, count);
    if (n < 0) {
        return SyscallResult(-errno);
    }
    return SyscallResult(n);
}

SyscallResult BionicDarwinTranslator::sys_close(int fd) {
    int host_fd = fd_table_.get(fd);
    if (host_fd >= 0) {
        ::close(host_fd);
        fd_table_.remove(fd);
    }
    return SyscallResult(0);
}

SyscallResult BionicDarwinTranslator::sys_lseek(int fd, off_t offset, 
                                                int whence) {
    int host_fd = fd_table_.get(fd);
    if (host_fd < 0) {
        return SyscallResult(-EBADF);
    }
    
    off_t result = ::lseek(host_fd, offset, whence);
    if (result < 0) {
        return SyscallResult(-errno);
    }
    return SyscallResult(result);
}

SyscallResult BionicDarwinTranslator::sys_mmap(void* addr, size_t length,
                                                int prot, int flags,
                                                int fd, off_t offset) {
    // Direct passthrough to macOS mmap (mostly compatible)
    void* result = ::mmap(addr, length, prot, flags, fd, offset);
    if (result == MAP_FAILED) {
        return SyscallResult(-errno);
    }
    return SyscallResult(reinterpret_cast<int64_t>(result));
}

SyscallResult BionicDarwinTranslator::sys_munmap(void* addr, size_t length) {
    int result = ::munmap(addr, length);
    if (result < 0) {
        return SyscallResult(-errno);
    }
    return SyscallResult(0);
}

SyscallResult BionicDarwinTranslator::sys_getpid() {
    return SyscallResult(::getpid());
}

SyscallResult BionicDarwinTranslator::sys_gettid() {
    // macOS doesn't have gettid, use pthread
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return SyscallResult(static_cast<int64_t>(tid));
}

SyscallResult BionicDarwinTranslator::sys_exit(int code) {
    ::exit(code);
    return SyscallResult(0);  // Never reached
}

SyscallResult BionicDarwinTranslator::sys_gettime(int clock_id, 
                                                  struct timespec* tp) {
    clockid_t host_clock;
    switch (clock_id) {
        case 0: host_clock = CLOCK_REALTIME; break;
        case 1: host_clock = CLOCK_MONOTONIC; break;
        default: host_clock = CLOCK_REALTIME;
    }
    
    int result = clock_gettime(host_clock, tp);
    if (result < 0) {
        return SyscallResult(-errno);
    }
    return SyscallResult(0);
}

int BionicDarwinTranslator::property_get(const std::string& key, 
                                          char* value, size_t len) {
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        strncpy(value, it->second.c_str(), len - 1);
        value[len - 1] = '\0';
        return strlen(value);
    }
    value[0] = '\0';
    return 0;
}

int BionicDarwinTranslator::property_set(const std::string& key, 
                                        const std::string& value) {
    properties_[key] = value;
    return 0;
}

void BionicDarwinTranslator::initIoctlMap() {
    // Linux → macOS ioctl code translation
    // These are often different between platforms
    ioctl_map_[0x5401] = TIOCMGET;  // TCGETS
    ioctl_map_[0x5402] = TIOCMSET;  // TCSETS
    // ... more mappings needed
}

// Path redirection implementation
void PathRedirection::setAppDataPath(const std::string& android_path,
                                     const std::string& host_path) {
    redirects_[android_path] = host_path;
}

std::string PathRedirection::redirect(const std::string& android_path) const {
    for (const auto& [prefix, replacement] : redirects_) {
        if (android_path.find(prefix) == 0) {
            return replacement + android_path.substr(prefix.length());
        }
    }
    return android_path;
}

// FD table implementation
int FDTable::map(int android_fd) {
    return get(android_fd);
}

int FDTable::get(int android_fd) const {
    auto it = fd_map_.find(android_fd);
    if (it != fd_map_.end()) {
        return it->second;
    }
    return -1;
}

void FDTable::set(int android_fd, int host_fd) {
    fd_map_[android_fd] = host_fd;
}

void FDTable::remove(int android_fd) {
    fd_map_.erase(android_fd);
}

int FDTable::allocate() {
    return next_fd_++;
}

// Global access
BionicDarwinTranslator* getSyscallTranslator() {
    if (!g_translator) {
        g_translator = std::make_unique<BionicDarwinTranslator>();
    }
    return g_translator.get();
}

} // namespace runtime
} // namespace apkx
