#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace apkx {
namespace runtime {

// Android system services stubs
// These simulate Android's system services for app compatibility

// Intent - Android IPC mechanism
struct Intent {
    std::string action;
    std::string data;
    std::string type;
    std::string component;  // package/class
    std::string package;
    std::vector<std::string> categories;
    std::map<std::string, std::string> extras;
    int flags = 0;
    
    void setAction(const std::string& a) { action = a; }
    void setClassName(const std::string& pkg, const std::string& cls);
    void setPackage(const std::string& p) { package = p; }
    void addCategory(const std::string& cat) { categories.push_back(cat); }
    void putExtra(const std::string& key, const std::string& value);
};

// Activity lifecycle states
enum class ActivityState {
    CREATED,
    STARTED,
    RESUMED,
    PAUSED,
    STOPPED,
    DESTROYED
};

// Activity record
struct ActivityRecord {
    int id;
    std::string class_name;
    std::string package_name;
    ActivityState state;
    std::unique_ptr<Intent> intent;
    void* instance = nullptr;  // Native object
    
    ActivityRecord(int i, const std::string& cls, const std::string& pkg)
        : id(i), class_name(cls), package_name(pkg), 
          state(ActivityState::CREATED) {}
};

// ActivityManager - manages app lifecycle
class ActivityManager {
public:
    ActivityManager();
    ~ActivityManager();
    
    // Start an activity
    bool startActivity(const Intent& intent);
    bool startActivity(const std::string& class_name);
    
    // Lifecycle callbacks (called by runtime)
    void onCreate(ActivityRecord* activity);
    void onStart(ActivityRecord* activity);
    void onResume(ActivityRecord* activity);
    void onPause(ActivityRecord* activity);
    void onStop(ActivityRecord* activity);
    void onDestroy(ActivityRecord* activity);
    
    // Get current/resumed activity
    ActivityRecord* getCurrentActivity();
    
    // Finish activity
    void finishActivity(int id);
    
    // Debug
    void dump();
    int getActivityCount() const;

private:
    std::vector<std::unique_ptr<ActivityRecord>> activities_;
    int next_activity_id_ = 1;
    ActivityRecord* current_activity_ = nullptr;
    
    ActivityRecord* findActivity(const std::string& class_name);
};

// PackageManager - app package info
class PackageManager {
public:
    PackageManager();
    
    struct PackageInfo {
        std::string package_name;
        std::string app_name;
        int version_code;
        std::string version_name;
        std::string apk_path;
        std::vector<std::string> permissions;
        std::string main_activity;
        std::vector<std::string> activities;
        std::vector<std::string> services;
    };
    
    bool installPackage(const std::string& apk_path);
    bool uninstallPackage(const std::string& package_name);
    PackageInfo* getPackageInfo(const std::string& package_name);
    std::vector<PackageInfo> getInstalledPackages();
    
    // Permission checking
    bool checkPermission(const std::string& package_name,
                        const std::string& permission);

private:
    std::map<std::string, PackageInfo> packages_;
};

// WindowManager - manages app windows (stubs for now)
class WindowManager {
public:
    WindowManager();
    
    // Window creation
    void* createWindow(int width, int height, const std::string& title);
    void destroyWindow(void* window);
    void setTitle(void* window, const std::string& title);
    void setSize(void* window, int width, int height);
    
    // Surface for rendering
    void* getSurface(void* window);
    void swapBuffers(void* window);
    
    // Input (to be translated to Android events)
    using InputCallback = std::function<void(int event_type, float x, float y)>;
    void setInputCallback(void* window, InputCallback callback);

private:
    std::map<void*, InputCallback> input_callbacks_;
};

// ServiceManager - manages background services
class ServiceManager {
public:
    ServiceManager();
    
    struct ServiceRecord {
        std::string class_name;
        std::string package_name;
        bool running = false;
        void* instance = nullptr;
    };
    
    bool startService(const Intent& intent);
    bool stopService(const Intent& intent);
    bool bindService(const Intent& intent, void* connection);
    void unbindService(void* connection);
    
    ServiceRecord* getService(const std::string& class_name);

private:
    std::map<std::string, ServiceRecord> services_;
};

// All system services container
class SystemServices {
public:
    SystemServices();
    ~SystemServices();
    
    bool initialize();
    
    // Access individual services
    ActivityManager* getActivityManager() { return activity_mgr_.get(); }
    PackageManager* getPackageManager() { return package_mgr_.get(); }
    WindowManager* getWindowManager() { return window_mgr_.get(); }
    ServiceManager* getServiceManager() { return service_mgr_.get(); }

private:
    std::unique_ptr<ActivityManager> activity_mgr_;
    std::unique_ptr<PackageManager> package_mgr_;
    std::unique_ptr<WindowManager> window_mgr_;
    std::unique_ptr<ServiceManager> service_mgr_;
};

} // namespace runtime
} // namespace apkx
