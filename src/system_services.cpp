#include "apkx/system_services.hpp"

#include <iostream>
#include <chrono>

namespace apkx {
namespace runtime {

// Intent implementation
void Intent::setClassName(const std::string& pkg, const std::string& cls) {
    component = pkg + "/" + cls;
    package = pkg;
}

void Intent::putExtra(const std::string& key, const std::string& value) {
    extras[key] = value;
}

// ActivityManager implementation
ActivityManager::ActivityManager() = default;
ActivityManager::~ActivityManager() = default;

bool ActivityManager::startActivity(const Intent& intent) {
    std::cout << "[*] ActivityManager.startActivity: " << intent.component << "\n";
    
    std::string class_name = intent.component;
    size_t slash = class_name.find('/');
    if (slash != std::string::npos) {
        class_name = class_name.substr(slash + 1);
    }
    
    // Check if activity exists
    ActivityRecord* existing = findActivity(class_name);
    if (existing) {
        // Bring to front
        std::cout << "    Bringing existing activity to front\n";
        current_activity_ = existing;
        return true;
    }
    
    // Create new activity record
    auto record = std::make_unique<ActivityRecord>(
        next_activity_id_++, class_name, intent.package);
    record->intent = std::make_unique<Intent>(intent);
    
    // Run lifecycle
    onCreate(record.get());
    onStart(record.get());
    onResume(record.get());
    
    current_activity_ = record.get();
    activities_.push_back(std::move(record));
    
    std::cout << "[+] Activity started: " << class_name << " (ID: " 
              << current_activity_->id << ")\n";
    return true;
}

bool ActivityManager::startActivity(const std::string& class_name) {
    Intent intent;
    intent.setClassName("com.app.package", class_name);
    intent.setAction("android.intent.action.MAIN");
    intent.addCategory("android.intent.category.LAUNCHER");
    return startActivity(intent);
}

void ActivityManager::onCreate(ActivityRecord* activity) {
    if (!activity) return;
    activity->state = ActivityState::CREATED;
    std::cout << "    -> onCreate: " << activity->class_name << "\n";
    
    // TODO: Call actual Activity.onCreate() through JNI
}

void ActivityManager::onStart(ActivityRecord* activity) {
    if (!activity) return;
    activity->state = ActivityState::STARTED;
    std::cout << "    -> onStart: " << activity->class_name << "\n";
}

void ActivityManager::onResume(ActivityRecord* activity) {
    if (!activity) return;
    activity->state = ActivityState::RESUMED;
    std::cout << "    -> onResume: " << activity->class_name << "\n";
}

void ActivityManager::onPause(ActivityRecord* activity) {
    if (!activity) return;
    activity->state = ActivityState::PAUSED;
    std::cout << "    -> onPause: " << activity->class_name << "\n";
}

void ActivityManager::onStop(ActivityRecord* activity) {
    if (!activity) return;
    activity->state = ActivityState::STOPPED;
    std::cout << "    -> onStop: " << activity->class_name << "\n";
}

void ActivityManager::onDestroy(ActivityRecord* activity) {
    if (!activity) return;
    activity->state = ActivityState::DESTROYED;
    std::cout << "    -> onDestroy: " << activity->class_name << "\n";
}

ActivityRecord* ActivityManager::getCurrentActivity() {
    return current_activity_;
}

void ActivityManager::finishActivity(int id) {
    for (auto it = activities_.begin(); it != activities_.end(); ++it) {
        if ((*it)->id == id) {
            onPause(it->get());
            onStop(it->get());
            onDestroy(it->get());
            activities_.erase(it);
            std::cout << "[*] Activity " << id << " finished\n";
            return;
        }
    }
}

ActivityRecord* ActivityManager::findActivity(const std::string& class_name) {
    for (auto& act : activities_) {
        if (act->class_name == class_name) {
            return act.get();
        }
    }
    return nullptr;
}

void ActivityManager::dump() {
    std::cout << "\n=== Activity Stack ===\n";
    std::cout << "Activities: " << activities_.size() << "\n";
    for (const auto& act : activities_) {
        std::cout << "  [" << act->id << "] " << act->class_name;
        switch (act->state) {
            case ActivityState::CREATED: std::cout << " (created)"; break;
            case ActivityState::STARTED: std::cout << " (started)"; break;
            case ActivityState::RESUMED: std::cout << " (RESUMED)"; break;
            case ActivityState::PAUSED: std::cout << " (paused)"; break;
            case ActivityState::STOPPED: std::cout << " (stopped)"; break;
            case ActivityState::DESTROYED: std::cout << " (destroyed)"; break;
        }
        if (act.get() == current_activity_) {
            std::cout << " [CURRENT]";
        }
        std::cout << "\n";
    }
}

int ActivityManager::getActivityCount() const {
    return static_cast<int>(activities_.size());
}

// PackageManager implementation
PackageManager::PackageManager() = default;

bool PackageManager::installPackage(const std::string& apk_path) {
    std::cout << "[*] PackageManager.installPackage: " << apk_path << "\n";
    // TODO: Parse APK, extract manifest, register package
    return true;
}

bool PackageManager::uninstallPackage(const std::string& package_name) {
    std::cout << "[*] PackageManager.uninstallPackage: " << package_name << "\n";
    packages_.erase(package_name);
    return true;
}

PackageManager::PackageInfo* PackageManager::getPackageInfo(
    const std::string& package_name) {
    auto it = packages_.find(package_name);
    if (it != packages_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<PackageManager::PackageInfo> PackageManager::getInstalledPackages() {
    std::vector<PackageInfo> result;
    for (auto& [name, info] : packages_) {
        result.push_back(info);
    }
    return result;
}

bool PackageManager::checkPermission(const std::string& package_name,
                                     const std::string& permission) {
    auto* pkg = getPackageInfo(package_name);
    if (!pkg) return false;
    
    for (const auto& p : pkg->permissions) {
        if (p == permission) return true;
    }
    return false;
}

// WindowManager implementation
WindowManager::WindowManager() = default;

void* WindowManager::createWindow(int width, int height, 
                                  const std::string& title) {
    std::cout << "[*] WindowManager.createWindow: " << width << "x" 
              << height << " \"" << title << "\"\n";
    // TODO: Create actual macOS window
    void* window = reinterpret_cast<void*>(0x1);  // Stub
    return window;
}

void WindowManager::destroyWindow(void* window) {
    std::cout << "[*] WindowManager.destroyWindow\n";
}

void WindowManager::setTitle(void* window, const std::string& title) {
    // TODO: Set window title
}

void WindowManager::setSize(void* window, int width, int height) {
    // TODO: Resize window
}

void* WindowManager::getSurface(void* window) {
    // TODO: Get Metal surface
    return nullptr;
}

void WindowManager::swapBuffers(void* window) {
    // TODO: Swap buffers
}

void WindowManager::setInputCallback(void* window, InputCallback callback) {
    input_callbacks_[window] = callback;
}

// ServiceManager implementation
ServiceManager::ServiceManager() = default;

bool ServiceManager::startService(const Intent& intent) {
    std::cout << "[*] ServiceManager.startService: " << intent.component << "\n";
    // TODO: Start background service
    return true;
}

bool ServiceManager::stopService(const Intent& intent) {
    std::cout << "[*] ServiceManager.stopService: " << intent.component << "\n";
    // TODO: Stop service
    return true;
}

bool ServiceManager::bindService(const Intent& intent, void* connection) {
    std::cout << "[*] ServiceManager.bindService\n";
    // TODO: Bind service
    return true;
}

void ServiceManager::unbindService(void* connection) {
    std::cout << "[*] ServiceManager.unbindService\n";
}

ServiceManager::ServiceRecord* ServiceManager::getService(
    const std::string& class_name) {
    auto it = services_.find(class_name);
    if (it != services_.end()) {
        return &it->second;
    }
    return nullptr;
}

// SystemServices implementation
SystemServices::SystemServices() = default;
SystemServices::~SystemServices() = default;

bool SystemServices::initialize() {
    std::cout << "[*] SystemServices initializing...\n";
    
    activity_mgr_ = std::make_unique<ActivityManager>();
    package_mgr_ = std::make_unique<PackageManager>();
    window_mgr_ = std::make_unique<WindowManager>();
    service_mgr_ = std::make_unique<ServiceManager>();
    
    std::cout << "    ActivityManager: OK\n";
    std::cout << "    PackageManager: OK\n";
    std::cout << "    WindowManager: OK\n";
    std::cout << "    ServiceManager: OK\n";
    
    return true;
}

} // namespace runtime
} // namespace apkx
