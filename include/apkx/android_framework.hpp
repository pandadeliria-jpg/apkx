#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>
#include <iostream>
#include <unistd.h>

namespace apkx {
namespace runtime {

// Android Context stub - every Android app has a Context
class Context {
public:
    virtual ~Context() = default;
    
    virtual std::string getPackageName() const { return ""; }
    virtual std::string getFilesDir() const { return ""; }
    virtual std::string getCacheDir() const { return ""; }
    virtual std::string getExternalFilesDir() const { return ""; }
    virtual void* getSystemService(const std::string& name) { return nullptr; }
    virtual int getSharedPrefs(const std::string& name) { return 0; }
};

// Bundle for passing data between components
class Bundle {
public:
    Bundle() = default;
    
    void putString(const std::string& key, const std::string& value) { strings_[key] = value; }
    void putInt(const std::string& key, int value) { ints_[key] = value; }
    void putLong(const std::string& key, int64_t value) { longs_[key] = value; }
    void putBoolean(const std::string& key, bool value) { bools_[key] = value; }
    
    std::string getString(const std::string& key) const {
        auto it = strings_.find(key);
        return it != strings_.end() ? it->second : "";
    }
    int getInt(const std::string& key, int default_val = 0) const {
        auto it = ints_.find(key);
        return it != ints_.end() ? it->second : default_val;
    }
    int64_t getLong(const std::string& key, int64_t default_val = 0) const {
        auto it = longs_.find(key);
        return it != longs_.end() ? it->second : default_val;
    }
    bool getBoolean(const std::string& key, bool default_val = false) const {
        auto it = bools_.find(key);
        return it != bools_.end() ? it->second : default_val;
    }
    
    bool containsKey(const std::string& key) const {
        return strings_.count(key) || ints_.count(key) || longs_.count(key) || bools_.count(key);
    }
    
private:
    std::map<std::string, std::string> strings_;
    std::map<std::string, int> ints_;
    std::map<std::string, int64_t> longs_;
    std::map<std::string, bool> bools_;
};

// Android View base class
class View {
public:
    View() = default;
    virtual ~View() = default;
    
    enum class Visibility { VISIBLE = 0, INVISIBLE = 4, GONE = 8 };
    
    virtual void setVisibility(Visibility v) { visibility_ = v; }
    virtual Visibility getVisibility() const { return visibility_; }
    
    virtual void setLayoutParams(int width, int height) { 
        width_ = width; height_ = height; 
    }
    virtual void setPadding(int left, int top, int right, int bottom) {
        padding_left_ = left; padding_top_ = top;
        padding_right_ = right; padding_bottom_ = bottom;
    }
    
    virtual void setOnClickListener(std::function<void()> listener) {
        on_click_ = listener;
    }
    
    virtual void setText(const std::string& text) { text_ = text; }
    virtual std::string getText() const { return text_; }
    
    virtual void invalidate() {}
    virtual void requestLayout() {}
    
protected:
    Visibility visibility_ = Visibility::VISIBLE;
    int width_ = -1;
    int height_ = -1;
    int padding_left_ = 0, padding_top_ = 0, padding_right_ = 0, padding_bottom_ = 0;
    std::string text_;
    std::function<void()> on_click_;
};

// TextView extends View
class TextView : public View {
public:
    TextView() = default;
    virtual ~TextView() = default;
    
    void setText(const std::string& text) override { text_ = text; }
    void setTextSize(float size) { text_size_ = size; }
    void setTextColor(uint32_t color) { text_color_ = color; }
    void setGravity(int gravity) { gravity_ = gravity; }
    
private:
    float text_size_ = 14.0f;
    uint32_t text_color_ = 0xFF000000;
    int gravity_ = 0x01; // left
};

// Button extends TextView
class Button : public TextView {
public:
    Button() = default;
    virtual ~Button() = default;
};

// ViewGroup contains Views
class ViewGroup : public View {
public:
    ViewGroup() = default;
    virtual ~ViewGroup() = default;
    
    void addView(View* child) { children_.push_back(child); }
    void removeView(View* child) {
        for (auto it = children_.begin(); it != children_.end(); ++it) {
            if (*it == child) { children_.erase(it); break; }
        }
    }
    void removeAllViews() { children_.clear(); }
    
    const std::vector<View*>& getChildren() const { return children_; }
    
private:
    std::vector<View*> children_;
};

// FrameLayout extends ViewGroup
class FrameLayout : public ViewGroup {
public:
    FrameLayout() = default;
    virtual ~FrameLayout() = default;
};

// LinearLayout extends ViewGroup
class LinearLayout : public ViewGroup {
public:
    LinearLayout() = default;
    virtual ~LinearLayout() = default;
    
    enum class Orientation { HORIZONTAL, VERTICAL };
    
    void setOrientation(Orientation o) { orientation_ = o; }
    Orientation getOrientation() const { return orientation_; }
    
    void setGravity(int g) { gravity_ = g; }
    
private:
    Orientation orientation_ = Orientation::VERTICAL;
    int gravity_ = 0x01 | 0x04 | 0x08; // left, top, bottom
};

// RelativeLayout extends ViewGroup
class RelativeLayout : public ViewGroup {
public:
    RelativeLayout() = default;
    virtual ~RelativeLayout() = default;
};

// Activity is the main component
class Activity {
public:
    Activity() = default;
    virtual ~Activity() = default;
    
    virtual void onCreate(Bundle* savedInstanceState) {
        std::cout << "[Activity] onCreate()\n";
    }
    virtual void onStart() {
        std::cout << "[Activity] onStart()\n";
    }
    virtual void onResume() {
        std::cout << "[Activity] onResume()\n";
    }
    virtual void onPause() {
        std::cout << "[Activity] onPause()\n";
    }
    virtual void onStop() {
        std::cout << "[Activity] onStop()\n";
    }
    virtual void onDestroy() {
        std::cout << "[Activity] onDestroy()\n";
    }
    virtual void onRestart() {
        std::cout << "[Activity] onRestart()\n";
    }
    
    virtual void setContentView(View* view) {
        content_view_ = view;
        std::cout << "[Activity] setContentView()\n";
    }
    
    virtual void setContentView(int layoutResId) {
        std::cout << "[Activity] setContentView(layoutId=" << layoutResId << ")\n";
        // Would inflate layout XML
    }
    
    virtual View* findViewById(int id) {
        // Would traverse view hierarchy
        return nullptr;
    }
    
    virtual Context* getContext() { return context_.get(); }
    virtual std::string getPackageName() const { return package_name_; }
    virtual std::string getFilesDir() const { return files_dir_; }
    
    void setPackageName(const std::string& name) { package_name_ = name; }
    void setFilesDir(const std::string& dir) { files_dir_ = dir; }
    void setContext(std::unique_ptr<Context> ctx) { context_ = std::move(ctx); }
    
protected:
    std::unique_ptr<Context> context_;
    View* content_view_ = nullptr;
    std::string package_name_;
    std::string files_dir_;
};

// Handler for posting runnables (like Android's Handler)
class Handler {
public:
    Handler() = default;
    virtual ~Handler() = default;
    
    virtual void post(std::function<void()> runnable) {
        pending_.push_back(runnable);
    }
    
    virtual void postDelayed(std::function<void()> runnable, int64_t delay_ms) {
        std::cout << "[Handler] postDelayed(" << delay_ms << "ms)\n";
    }
    
    virtual void removeCallbacks(std::function<void()> runnable) {
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it->target<Handler>() == runnable.target<Handler>()) {
                pending_.erase(it);
                break;
            }
        }
    }
    
    void process() {
        for (auto& r : pending_) {
            r();
        }
        pending_.clear();
    }
    
private:
    std::vector<std::function<void()>> pending_;
};

// Looper runs the message loop (like Android's Looper)
class Looper {
public:
    static Looper* getMainLooper() {
        static Looper main_looper;
        return &main_looper;
    }
    
    static Looper* myLooper() {
        return current_looper_;
    }
    
    void prepare() {
        current_looper_ = this;
        std::cout << "[Looper] prepare()\n";
    }
    
    void loop() {
        std::cout << "[Looper] loop() - entering message pump\n";
        // Would process messages until quit()
        running_ = true;
        while (running_) {
            // Process messages
            usleep(1000); // Prevent busy loop
        }
        std::cout << "[Looper] loop() - exiting\n";
    }
    
    void quit() {
        running_ = false;
    }
    
    void quitSafely() {
        running_ = false;
    }
    
    Handler* getThreadHandler() {
        return &handler_;
    }
    
private:
    static thread_local Looper* current_looper_;
    bool running_ = false;
    Handler handler_;
};

thread_local Looper* Looper::current_looper_ = nullptr;

// Log - Android's logging system
class Log {
public:
    static int d(const char* tag, const char* msg) {
        std::cout << "[D/" << tag << "] " << msg << "\n";
        return 0;
    }
    static int i(const char* tag, const char* msg) {
        std::cout << "[I/" << tag << "] " << msg << "\n";
        return 0;
    }
    static int w(const char* tag, const char* msg) {
        std::cerr << "[W/" << tag << "] " << msg << "\n";
        return 0;
    }
    static int e(const char* tag, const char* msg) {
        std::cerr << "[E/" << tag << "] " << msg << "\n";
        return -1;
    }
};

// SharedPreferences for simple key-value storage
class SharedPreferences {
public:
    virtual ~SharedPreferences() = default;
    
    virtual bool contains(const std::string& key) const = 0;
    virtual std::string getString(const std::string& key, const std::string& def = "") const = 0;
    virtual int getInt(const std::string& key, int def = 0) const = 0;
    virtual int64_t getLong(const std::string& key, int64_t def = 0) const = 0;
    virtual bool getBoolean(const std::string& key, bool def = false) const = 0;
    virtual float getFloat(const std::string& key, float def = 0.0f) const = 0;
};

// SharedPreferences.Editor for modifications
class SharedPreferencesEditor {
public:
    virtual ~SharedPreferencesEditor() = default;
    
    virtual void putString(const std::string& key, const std::string& value) = 0;
    virtual void putInt(const std::string& key, int value) = 0;
    virtual void putLong(const std::string& key, int64_t value) = 0;
    virtual void putBoolean(const std::string& key, bool value) = 0;
    virtual void putFloat(const std::string& key, float value) = 0;
    virtual void remove(const std::string& key) = 0;
    virtual void clear() = 0;
    virtual bool commit() = 0;
};

// Bundle android content stubs
namespace android {
namespace content {

class Intent {
public:
    Intent() = default;
    Intent(const std::string& action) : action_(action) {}
    
    void setAction(const std::string& action) { action_ = action; }
    std::string getAction() const { return action_; }
    
    void setClassName(const std::string& pkg, const std::string& cls) {
        component_ = pkg + "/" + cls;
    }
    std::string getComponent() const { return component_; }
    
    void setPackage(const std::string& pkg) { package_ = pkg; }
    std::string getPackage() const { return package_; }
    
    void putExtra(const std::string& key, const std::string& value) { extras_[key] = value; }
    void putExtra(const std::string& key, int value) { extras_int_[key] = value; }
    
    std::string getStringExtra(const std::string& key) const {
        auto it = extras_.find(key);
        return it != extras_.end() ? it->second : "";
    }
    int getIntExtra(const std::string& key, int def = 0) const {
        auto it = extras_int_.find(key);
        return it != extras_int_.end() ? it->second : def;
    }
    
    static const std::string ACTION_MAIN;
    static const std::string ACTION_VIEW;
    static const std::string ACTION_SEND;
    
private:
    std::string action_;
    std::string component_;
    std::string package_;
    std::map<std::string, std::string> extras_;
    std::map<std::string, int> extras_int_;
};

} // namespace content
} // namespace android

} // namespace runtime
} // namespace apkx