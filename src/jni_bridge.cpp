#include "apkx/jni_bridge.hpp"

#include <iostream>
#include <dlfcn.h>
#include <cstring>

namespace apkx {
namespace runtime {

// NativeLibrary implementation
NativeLibrary::NativeLibrary(const std::string& path) : path_(path) {}

NativeLibrary::~NativeLibrary() {
    unload();
}

bool NativeLibrary::load() {
    if (loaded_) return true;
    
    handle_ = dlopen(path_.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle_) {
        std::cerr << "[!] dlopen failed: " << dlerror() << "\n";
        return false;
    }
    
    loaded_ = true;
    std::cout << "[*] Loaded native lib: " << path_ << "\n";
    
    // Check for JNI_OnLoad
    auto* onload = reinterpret_cast<int(*)(JavaVM*, void*)>(
        dlsym(handle_, "JNI_OnLoad"));
    if (onload) {
        std::cout << "    JNI_OnLoad found\n";
    }
    
    return true;
}

void NativeLibrary::unload() {
    if (handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
    loaded_ = false;
}

void* NativeLibrary::getSymbol(const std::string& name) {
    if (!loaded_) return nullptr;
    return dlsym(handle_, name.c_str());
}

// JNIEnvironment implementation
JNIEnvironment::JNIEnvironment() = default;
JNIEnvironment::~JNIEnvironment() = default;

bool JNIEnvironment::initialize() {
    std::cout << "[*] JNIEnvironment initializing...\n";
    
    initializeBuiltinMethods();
    
    std::cout << "    Registered methods: " << registered_methods_.size() << "\n";
    return true;
}

void JNIEnvironment::initializeBuiltinMethods() {
    // Register common Android system methods
    
    // System.currentTimeMillis()
    registerMethod("java/lang/System", "currentTimeMillis", "()J",
        [](JNIEnv* env, jobject thiz, const std::vector<void*>& args) {
            // Return current time in milliseconds
            std::cout << "[JNI] System.currentTimeMillis()\n";
        });
    
    // System.nanoTime()
    registerMethod("java/lang/System", "nanoTime", "()J",
        [](JNIEnv* env, jobject thiz, const std::vector<void*>& args) {
            std::cout << "[JNI] System.nanoTime()\n";
        });
    
    // Log.d()
    registerMethod("android/util/Log", "d", 
        "(Ljava/lang/String;Ljava/lang/String;)I",
        [](JNIEnv* env, jobject thiz, const std::vector<void*>& args) {
            std::cout << "[JNI] Log.d()\n";
        });
    
    // Looper.prepare()
    registerMethod("android/os/Looper", "prepare", "()V",
        [](JNIEnv* env, jobject thiz, const std::vector<void*>& args) {
            std::cout << "[JNI] Looper.prepare()\n";
        });
    
    // Looper.loop()
    registerMethod("android/os/Looper", "loop", "()V",
        [](JNIEnv* env, jobject thiz, const std::vector<void*>& args) {
            std::cout << "[JNI] Looper.loop() - would block forever\n";
        });

    // Build.VERSION.SDK_INT
    registerMethod("android/os/Build$VERSION", "SDK_INT", "I",
        [](JNIEnv* env, jobject thiz, const std::vector<void*>& args) {
            std::cout << "[JNI] Build.VERSION.SDK_INT -> 33\n";
        });

    // Settings.Secure.getString
    registerMethod("android/provider/Settings$Secure", "getString", 
        "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
        [](JNIEnv* env, jobject thiz, const std::vector<void*>& args) {
            std::cout << "[JNI] Settings.Secure.getString()\n";
        });

    // Context.getSystemService
    registerMethod("android/content/Context", "getSystemService", 
        "(Ljava/lang/String;)Ljava/lang/Object;",
        [](JNIEnv* env, jobject thiz, const std::vector<void*>& args) {
            std::cout << "[JNI] Context.getSystemService()\n";
        });
}

void JNIEnvironment::registerMethod(const std::string& class_name,
                                  const std::string& method_name,
                                  const std::string& signature,
                                  NativeMethod func) {
    std::string full_sig = class_name + "." + method_name + signature;
    registered_methods_[full_sig] = func;
    method_cache_[class_name][method_name] = func;
}

bool JNIEnvironment::callNative(const std::string& full_signature,
                               jobject thiz,
                               const std::vector<void*>& args) {
    auto it = registered_methods_.find(full_signature);
    if (it != registered_methods_.end()) {
        it->second(nullptr, thiz, args);
        return true;
    }
    
    std::cerr << "[!] Unimplemented JNI method: " << full_signature << "\n";
    return false;
}

NativeMethod JNIEnvironment::findMethod(const std::string& class_name,
                                     const std::string& method_name) {
    auto it = method_cache_.find(class_name);
    if (it != method_cache_.end()) {
        auto mit = it->second.find(method_name);
        if (mit != it->second.end()) {
            return mit->second;
        }
    }
    return nullptr;
}

bool JNIEnvironment::loadLibrary(const std::string& lib_name,
                                const std::vector<std::string>& search_paths) {
    std::cout << "[*] Loading JNI library: " << lib_name << "\n";
    
    // Try to find the library in search paths
    for (const auto& path : search_paths) {
        std::string full_path = path + "/" + lib_name;
        
        auto lib = std::make_unique<NativeLibrary>(full_path);
        if (lib->load()) {
            loaded_libs_.push_back(std::move(lib));
            return true;
        }
    }
    
    // Try system paths
    auto lib = std::make_unique<NativeLibrary>(lib_name);
    if (lib->load()) {
        loaded_libs_.push_back(std::move(lib));
        return true;
    }
    
    std::cerr << "[!] Failed to load library: " << lib_name << "\n";
    return false;
}

jclass JNIEnvironment::findClass(const char* name) {
    // TODO: Implement class loading
    std::cout << "[JNI] FindClass: " << name << "\n";
    return nullptr;
}

jmethodID JNIEnvironment::getMethodID(jclass cls, const char* name, 
                                     const char* sig) {
    std::cout << "[JNI] GetMethodID: " << name << sig << "\n";
    return nullptr;
}

jobject JNIEnvironment::newObject(jclass cls, jmethodID methodID, ...) {
    std::cout << "[JNI] NewObject\n";
    return nullptr;
}

void JNIEnvironment::callVoidMethod(jobject obj, jmethodID methodID, ...) {
    std::cout << "[JNI] CallVoidMethod\n";
}

int JNIEnvironment::callIntMethod(jobject obj, jmethodID methodID, ...) {
    std::cout << "[JNI] CallIntMethod\n";
    return 0;
}

const char* JNIEnvironment::getStringUTFChars(jstring str, 
                                               jboolean* isCopy) {
    // TODO: Implement string conversion
    return "";
}

void JNIEnvironment::releaseStringUTFChars(jstring str, const char* chars) {
    // TODO: Implement
}

jobject JNIEnvironment::newString(const char* utf) {
    std::cout << "[JNI] NewStringUTF: " << utf << "\n";
    return nullptr;
}

jobject JNIEnvironment::newObjectArray(int32_t len, jclass cls, jobject init) {
    std::cout << "[JNI] NewObjectArray\n";
    return nullptr;
}

jobject JNIEnvironment::getObjectArrayElement(jobjectArray arr, 
                                               int32_t index) {
    return nullptr;
}

// JavaVM singleton implementation
AndroidJavaVM* AndroidJavaVM::getInstance() {
    static AndroidJavaVM instance;
    return &instance;
}

JNIEnv* AndroidJavaVM::getEnv() {
    static int dummy;
    return reinterpret_cast<JNIEnv*>(&dummy);
}

int AndroidJavaVM::attachCurrentThread(JNIEnv** p_env, void* thr_args) {
    *p_env = getEnv();
    return 0;
}

int AndroidJavaVM::detachCurrentThread() {
    return 0;
}

} // namespace runtime
} // namespace apkx
