#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <stack>
#include <unordered_map>

namespace apkx {
namespace runtime {

// Forward declare JavaVM from bionic_darwin.hpp
struct JavaVM;

// JNI Types (from jni.h)
typedef void* jobject;
typedef jobject* jobjectArray;
typedef void* jclass;
typedef void* jmethodID;
typedef void* jfieldID;
typedef uint8_t jboolean;
typedef int8_t jbyte;
typedef int16_t jshort;
typedef int32_t jint;
typedef int64_t jlong;
typedef float jfloat;
typedef double jdouble;
typedef uint16_t jchar;
typedef const char* jstring;
typedef const uint16_t* jcharArray;
typedef int32_t jsize;
typedef void* jthrowable;

enum class jobjectType {
    JNIInvalidRefType = 0,
    JNILocalRefType = 1,
    JNIGlobalRefType = 2,
    JNIWeakGlobalRefType = 3,
    JNILocalRefTypeCount = 4
};

typedef jobjectType jobjectRefType;

struct JNINativeMethod {
    const char* name;
    const char* signature;
    void* fnPtr;
};

// JNI version constants
#define JNI_VERSION_1_1 0x00010001
#define JNI_VERSION_1_2 0x00010002
#define JNI_VERSION_1_4 0x00010004
#define JNI_VERSION_1_6 0x00010006
#define JNI_VERSION_1_8 0x00010008
#define JNI_VERSION_9_0 0x00090000
#define JNI_VERSION_10_0 0x000a0000

// JNI primitive arrays
typedef int32_t* jintArray;
typedef int64_t* jlongArray;
typedef float* jfloatArray;
typedef double* jdoubleArray;
typedef uint8_t* jbyteArray;
typedef uint16_t* jshortArray;
typedef uint8_t* jbooleanArray;

// Forward declarations for internal objects
class JNIObject;
class JNIClass;
class JNIArray;
class JNIString;

// Forward declare JavaVM (already declared in bionic_darwin.hpp)
// class JavaVM_Internal;

// JNI type aliases - JNIEnv is pointer to function table
typedef void* JNIEnv;
// Use struct JavaVM from bionic_darwin.hpp

// Native method signature
using NativeMethod = std::function<void(JNIEnv*, jobject, const std::vector<void*>&)>;

// Native library handle
class NativeLibrary {
public:
    explicit NativeLibrary(const std::string& path);
    ~NativeLibrary();
    
    bool load();
    void unload();
    bool isLoaded() const { return loaded_; }
    
    // Find and call exported functions
    void* getSymbol(const std::string& name);
    
    // Register JNI methods
    void registerJNIMethod(const std::string& class_name,
                         const std::string& method_name,
                         const std::string& signature,
                         NativeMethod impl);
    
    // JNI_OnLoad support
    int callJNIOnLoad(void* vm, void* reserved);
    void callJNIOnUnload(void* vm, void* reserved);

private:
    std::string path_;
    void* handle_ = nullptr;
    bool loaded_ = false;
};

// JNI Environment implementation
class JNIEnvironment {
public:
    JNIEnvironment();
    ~JNIEnvironment();
    
    bool initialize();
    
    // Native method registration
    void registerMethod(const std::string& class_name,
                       const std::string& method_name,
                       const std::string& signature,
                       NativeMethod func);
    
    // Call native method
    bool callNative(const std::string& full_signature,
                   jobject thiz,
                   const std::vector<void*>& args);
    
    // Find method in loaded libraries
    NativeMethod findMethod(const std::string& class_name,
                           const std::string& method_name);
    
    // Load native library
    bool loadLibrary(const std::string& lib_name,
                     const std::vector<std::string>& search_paths);
    
    // JNI function implementations (called from interpreted code)
    jclass findClass(const char* name);
    jmethodID getMethodID(jclass cls, const char* name, const char* sig);
    jobject newObject(jclass cls, jmethodID methodID, ...);
    void callVoidMethod(jobject obj, jmethodID methodID, ...);
    int callIntMethod(jobject obj, jmethodID methodID, ...);
    const char* getStringUTFChars(jstring str, jboolean* isCopy);
    void releaseStringUTFChars(jstring str, const char* chars);
    
    // Object creation
    jobject newString(const char* utf);
    jobject newObjectArray(int32_t len, jclass cls, jobject init);
    jobject getObjectArrayElement(jobjectArray arr, int32_t index);

private:
    std::map<std::string, NativeMethod> registered_methods_;
    std::vector<std::unique_ptr<NativeLibrary>> loaded_libs_;
    
    // JNI method cache
    std::map<std::string, std::map<std::string, NativeMethod>> method_cache_;
    
    void initializeBuiltinMethods();
};

// JavaVM implementation (singleton per process)
class AndroidJavaVM {
public:
    static AndroidJavaVM* getInstance();
    
    JNIEnv* getEnv();
    int attachCurrentThread(JNIEnv** p_env, void* thr_args);
    int detachCurrentThread();

private:
    AndroidJavaVM() = default;
};

// JNI Function Table - Real JNI interface like Android's jni.h
// This is the key to fixing the bus error - functions must NOT be null
struct JNINativeInterface_ {
    // Version
    jint reserved0;
    jint reserved1;
    jint reserved2;
    jint reserved3;
    
    // Reference Management
    jobject (*NewGlobalRef)(JNIEnv*, jobject);
    void (*DeleteGlobalRef)(JNIEnv*, jobject);
    void (*DeleteLocalRef)(JNIEnv*, jobject);
    jobject (*NewLocalRef)(JNIEnv*, jobject);
    jobject (*EnsureLocalCapacity)(JNIEnv*, jint);
    
    // Object Operations
    jclass (*GetObjectClass)(JNIEnv*, jobject);
    jboolean (*IsInstanceOf)(JNIEnv*, jobject, jclass);
    jboolean (*IsSameObject)(JNIEnv*, jobject, jobject);
    
    // Class Operations
    jclass (*FindClass)(JNIEnv*, const char*);
    jmethodID (*GetMethodID)(JNIEnv*, jclass, const char*, const char*);
    jfieldID (*GetFieldID)(JNIEnv*, jclass, const char*, const char*);
    jmethodID (*GetStaticMethodID)(JNIEnv*, jclass, const char*, const char*);
    jfieldID (*GetStaticFieldID)(JNIEnv*, jclass, const char*, const char*);
    
    // Call Instance Methods
    jobject (*CallObjectMethod)(JNIEnv*, jobject, jmethodID, ...);
    jobject (*CallObjectMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jobject (*CallObjectMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    jboolean (*CallBooleanMethod)(JNIEnv*, jobject, jmethodID, ...);
    jboolean (*CallBooleanMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jboolean (*CallBooleanMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    jbyte (*CallByteMethod)(JNIEnv*, jobject, jmethodID, ...);
    jbyte (*CallByteMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jbyte (*CallByteMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    jchar (*CallCharMethod)(JNIEnv*, jobject, jmethodID, ...);
    jchar (*CallCharMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jchar (*CallCharMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    jshort (*CallShortMethod)(JNIEnv*, jobject, jmethodID, ...);
    jshort (*CallShortMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jshort (*CallShortMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    jint (*CallIntMethod)(JNIEnv*, jobject, jmethodID, ...);
    jint (*CallIntMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jint (*CallIntMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    jlong (*CallLongMethod)(JNIEnv*, jobject, jmethodID, ...);
    jlong (*CallLongMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jlong (*CallLongMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    jfloat (*CallFloatMethod)(JNIEnv*, jobject, jmethodID, ...);
    jfloat (*CallFloatMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jfloat (*CallFloatMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    jdouble (*CallDoubleMethod)(JNIEnv*, jobject, jmethodID, ...);
    jdouble (*CallDoubleMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    jdouble (*CallDoubleMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    void (*CallVoidMethod)(JNIEnv*, jobject, jmethodID, ...);
    void (*CallVoidMethodV)(JNIEnv*, jobject, jmethodID, va_list);
    void (*CallVoidMethodA)(JNIEnv*, jobject, jmethodID, const void*);
    
    // Call Non-virtual Methods
    jobject (*CallNonvirtualObjectMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jobject (*CallNonvirtualObjectMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jobject (*CallNonvirtualObjectMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    jboolean (*CallNonvirtualBooleanMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jboolean (*CallNonvirtualBooleanMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jboolean (*CallNonvirtualBooleanMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    jbyte (*CallNonvirtualByteMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jbyte (*CallNonvirtualByteMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jbyte (*CallNonvirtualByteMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    jchar (*CallNonvirtualCharMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jchar (*CallNonvirtualCharMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jchar (*CallNonvirtualCharMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    jshort (*CallNonvirtualShortMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jshort (*CallNonvirtualShortMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jshort (*CallNonvirtualShortMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    jint (*CallNonvirtualIntMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jint (*CallNonvirtualIntMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jint (*CallNonvirtualIntMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    jlong (*CallNonvirtualLongMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jlong (*CallNonvirtualLongMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jlong (*CallNonvirtualLongMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    jfloat (*CallNonvirtualFloatMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jfloat (*CallNonvirtualFloatMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jfloat (*CallNonvirtualFloatMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    jdouble (*CallNonvirtualDoubleMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jdouble (*CallNonvirtualDoubleMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jdouble (*CallNonvirtualDoubleMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    void (*CallNonvirtualVoidMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    void (*CallNonvirtualVoidMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    void (*CallNonvirtualVoidMethodA)(JNIEnv*, jobject, jclass, jmethodID, const void*);
    
    // Get Field Values
    jobject (*GetObjectField)(JNIEnv*, jobject, jfieldID);
    jboolean (*GetBooleanField)(JNIEnv*, jobject, jfieldID);
    jbyte (*GetByteField)(JNIEnv*, jobject, jfieldID);
    jchar (*GetCharField)(JNIEnv*, jobject, jfieldID);
    jshort (*GetShortField)(JNIEnv*, jobject, jfieldID);
    jint (*GetIntField)(JNIEnv*, jobject, jfieldID);
    jlong (*GetLongField)(JNIEnv*, jobject, jfieldID);
    jfloat (*GetFloatField)(JNIEnv*, jobject, jfieldID);
    jdouble (*GetDoubleField)(JNIEnv*, jobject, jfieldID);
    
    // Set Field Values
    void (*SetObjectField)(JNIEnv*, jobject, jfieldID, jobject);
    void (*SetBooleanField)(JNIEnv*, jobject, jfieldID, jboolean);
    void (*SetByteField)(JNIEnv*, jobject, jfieldID, jbyte);
    void (*SetCharField)(JNIEnv*, jobject, jfieldID, jchar);
    void (*SetShortField)(JNIEnv*, jobject, jfieldID, jshort);
    void (*SetIntField)(JNIEnv*, jobject, jfieldID, jint);
    void (*SetLongField)(JNIEnv*, jobject, jfieldID, jlong);
    void (*SetFloatField)(JNIEnv*, jobject, jfieldID, jfloat);
    void (*SetDoubleField)(JNIEnv*, jobject, jfieldID, jdouble);
    
    // Static Fields
    jobject (*GetStaticObjectField)(JNIEnv*, jclass, jfieldID);
    jboolean (*GetStaticBooleanField)(JNIEnv*, jclass, jfieldID);
    jbyte (*GetStaticByteField)(JNIEnv*, jclass, jfieldID);
    jchar (*GetStaticCharField)(JNIEnv*, jclass, jfieldID);
    jshort (*GetStaticShortField)(JNIEnv*, jclass, jfieldID);
    jint (*GetStaticIntField)(JNIEnv*, jclass, jfieldID);
    jlong (*GetStaticLongField)(JNIEnv*, jclass, jfieldID);
    jfloat (*GetStaticFloatField)(JNIEnv*, jclass, jfieldID);
    jdouble (*GetStaticDoubleField)(JNIEnv*, jclass, jfieldID);
    
    void (*SetStaticObjectField)(JNIEnv*, jclass, jfieldID, jobject);
    void (*SetStaticBooleanField)(JNIEnv*, jclass, jfieldID, jboolean);
    void (*SetStaticByteField)(JNIEnv*, jclass, jfieldID, jbyte);
    void (*SetStaticCharField)(JNIEnv*, jclass, jfieldID, jchar);
    void (*SetStaticShortField)(JNIEnv*, jclass, jfieldID, jshort);
    void (*SetStaticIntField)(JNIEnv*, jclass, jfieldID, jint);
    void (*SetStaticLongField)(JNIEnv*, jclass, jfieldID, jlong);
    void (*SetStaticFloatField)(JNIEnv*, jclass, jfieldID, jfloat);
    void (*SetStaticDoubleField)(JNIEnv*, jclass, jfieldID, jdouble);
    
    // String Operations
    jstring (*NewString)(JNIEnv*, const jchar*, jsize);
    jsize (*GetStringLength)(JNIEnv*, jstring);
    const jchar* (*GetStringChars)(JNIEnv*, jstring, jboolean*);
    void (*ReleaseStringChars)(JNIEnv*, jstring, const jchar*);
    jstring (*NewStringUTF)(JNIEnv*, const char*);
    jsize (*GetStringUTFLength)(JNIEnv*, jstring);
    const char* (*GetStringUTFChars)(JNIEnv*, jstring, jboolean*);
    void (*ReleaseStringUTFChars)(JNIEnv*, jstring, const char*);
    
    // Array Operations
    jobjectArray (*NewObjectArray)(JNIEnv*, jsize, jclass, jobject);
    jobject (*GetObjectArrayElement)(JNIEnv*, jobjectArray, jsize);
    void (*SetObjectArrayElement)(JNIEnv*, jobjectArray, jsize, jobject);
    jbooleanArray (*NewBooleanArray)(JNIEnv*, jsize);
    jbyteArray (*NewByteArray)(JNIEnv*, jsize);
    jcharArray (*NewCharArray)(JNIEnv*, jsize);
    jshortArray (*NewShortArray)(JNIEnv*, jsize);
    jintArray (*NewIntArray)(JNIEnv*, jsize);
    jlongArray (*NewLongArray)(JNIEnv*, jsize);
    jfloatArray (*NewFloatArray)(JNIEnv*, jsize);
    jdoubleArray (*NewDoubleArray)(JNIEnv*, jsize);
    
    jboolean* (*GetBooleanArrayElements)(JNIEnv*, jbooleanArray, jboolean*);
    jbyte* (*GetByteArrayElements)(JNIEnv*, jbyteArray, jboolean*);
    jchar* (*GetCharArrayElements)(JNIEnv*, jcharArray, jboolean*);
    jshort* (*GetShortArrayElements)(JNIEnv*, jshortArray, jboolean*);
    jint* (*GetIntArrayElements)(JNIEnv*, jintArray, jboolean*);
    jlong* (*GetLongArrayElements)(JNIEnv*, jlongArray, jboolean*);
    jfloat* (*GetFloatArrayElements)(JNIEnv*, jfloatArray, jboolean*);
    jdouble* (*GetDoubleArrayElements)(JNIEnv*, jdoubleArray, jboolean*);
    
    void (*ReleaseBooleanArrayElements)(JNIEnv*, jbooleanArray, jboolean*, jint);
    void (*ReleaseByteArrayElements)(JNIEnv*, jbyteArray, jbyte*, jint);
    void (*ReleaseCharArrayElements)(JNIEnv*, jcharArray, jchar*, jint);
    void (*ReleaseShortArrayElements)(JNIEnv*, jshortArray, jshort*, jint);
    void (*ReleaseIntArrayElements)(JNIEnv*, jintArray, jint*, jint);
    void (*ReleaseLongArrayElements)(JNIEnv*, jlongArray, jlong*, jint);
    void (*ReleaseFloatArrayElements)(JNIEnv*, jfloatArray, jfloat*, jint);
    void (*ReleaseDoubleArrayElements)(JNIEnv*, jdoubleArray, jdouble*, jint);
    
    void (*GetBooleanArrayRegion)(JNIEnv*, jbooleanArray, jsize, jsize, jboolean*);
    void (*GetByteArrayRegion)(JNIEnv*, jbyteArray, jsize, jsize, jbyte*);
    void (*GetCharArrayRegion)(JNIEnv*, jcharArray, jsize, jsize, jchar*);
    void (*GetShortArrayRegion)(JNIEnv*, jshortArray, jsize, jsize, jshort*);
    void (*GetIntArrayRegion)(JNIEnv*, jintArray, jsize, jsize, jint*);
    void (*GetLongArrayRegion)(JNIEnv*, jlongArray, jsize, jsize, jlong*);
    void (*GetFloatArrayRegion)(JNIEnv*, jfloatArray, jsize, jsize, jfloat*);
    void (*GetDoubleArrayRegion)(JNIEnv*, jdoubleArray, jsize, jsize, jdouble*);
    
    void (*SetBooleanArrayRegion)(JNIEnv*, jbooleanArray, jsize, jsize, const jboolean*);
    void (*SetByteArrayRegion)(JNIEnv*, jbyteArray, jsize, jsize, const jbyte*);
    void (*SetCharArrayRegion)(JNIEnv*, jcharArray, jsize, jsize, const jchar*);
    void (*SetShortArrayRegion)(JNIEnv*, jshortArray, jsize, jsize, const jshort*);
    void (*SetIntArrayRegion)(JNIEnv*, jintArray, jsize, jsize, const jint*);
    void (*SetLongArrayRegion)(JNIEnv*, jlongArray, jsize, jsize, const jlong*);
    void (*SetFloatArrayRegion)(JNIEnv*, jfloatArray, jsize, jsize, const jfloat*);
    void (*SetDoubleArrayRegion)(JNIEnv*, jdoubleArray, jsize, jsize, const jdouble*);
    
    // Object Creation
    jobject (*AllocObject)(JNIEnv*, jclass);
    jobject (*NewObject)(JNIEnv*, jclass, jmethodID, ...);
    jobject (*NewObjectV)(JNIEnv*, jclass, jmethodID, va_list);
    jobject (*NewObjectA)(JNIEnv*, jclass, jmethodID, const void*);
    
    // Exceptions
    jthrowable (*ExceptionOccurred)(JNIEnv*);
    void (*ExceptionDescribe)(JNIEnv*);
    void (*ExceptionClear)(JNIEnv*);
    void (*FatalError)(JNIEnv*, const char*);
    jint (*PushLocalFrame)(JNIEnv*, jint);
    jobject (*PopLocalFrame)(JNIEnv*, jobject);
    
    // Direct Buffer Management
    jobject (*NewDirectByteBuffer)(JNIEnv*, void*, jlong);
    void* (*GetDirectBufferAddress)(JNIEnv*, jobject);
    jlong (*GetDirectBufferCapacity)(JNIEnv*, jobject);
    
    // Reflection Support
    jobject (*GetSuperclass)(JNIEnv*, jclass);
    jboolean (*IsAssignableFrom)(JNIEnv*, jclass, jclass);
    // Removed duplicate IsSameObject
    
    // Native Method Registration
    void (*RegisterNatives)(JNIEnv*, jclass, const JNINativeMethod*, jint);
    void (*UnregisterNatives)(JNIEnv*, jclass);
    
    // Monitor Operations
    jint (*MonitorEnter)(JNIEnv*, jobject);
    jint (*MonitorExit)(JNIEnv*, jobject);
    
    // UTF-8 String Operations
    jint (*GetJavaVM)(JNIEnv*, void**);
    void (*GetStringRegion)(JNIEnv*, jstring, jsize, jsize, jchar*);
    void (*GetStringUTFRegion)(JNIEnv*, jstring, jsize, jsize, char*);
    
    // Weak References (JNI 1.2+)
    jobject (*NewWeakGlobalRef)(JNIEnv*, jobject);
    void (*DeleteWeakGlobalRef)(JNIEnv*, jobject);
    
    // Exception Check
    jboolean (*ExceptionCheck)(JNIEnv*);
    
    // Direct ByteBuffer (added in JDK 1.4)
    jobjectRefType (*GetObjectRefType)(JNIEnv*, jobject);
};

typedef JNINativeInterface_* JNIInvokeInterface;

// JavaVM function table
struct JavaVM_Internal_ {
    jint reserved0;
    jint reserved1;
    jint reserved2;
    jint reserved3;
    jint (*DestroyJavaVM)(void*);
    jint (*AttachCurrentThread)(void*, void**, void*);
    jint (*DetachCurrentThread)(void*);
    jint (*GetEnv)(void*, void**, jint);
    jint (*AttachCurrentThreadAsDaemon)(void*, void**, void*);
};

typedef JavaVM_Internal_* JavaVMInvokeInterface;

} // namespace runtime
} // namespace apkx
