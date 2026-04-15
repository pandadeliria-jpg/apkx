// Android UI to macOS native rendering
// Translates Android View hierarchy to AppKit UI elements

#import <Cocoa/Cocoa.h>
#include <string>
#include <vector>
#include <map>

namespace apkx {
namespace runtime {

// Android View types
enum class AndroidViewType {
    LinearLayout,
    RelativeLayout,
    FrameLayout,
    TextView,
    Button,
    ImageView,
    EditText,
    RecyclerView,
    Unknown
};

// Android View properties
struct AndroidView {
    AndroidViewType type = AndroidViewType::Unknown;
    std::string id;
    std::string text;
    float x = 0, y = 0;
    float width = 100, height = 50;
    uint32_t backgroundColor = 0xFFFFFFFF;
    uint32_t textColor = 0xFF000000;
    float textSize = 14.0f;
    std::vector<AndroidView> children;
};

// Parse APK resources and create view hierarchy
class AndroidUIRenderer {
public:
    AndroidUIRenderer() = default;
    
    // Parse Android manifest and resources to build UI
    bool loadFromAPK(const std::string& apkPath) {
        NSLog(@"[AndroidUI] Loading from APK: %s", apkPath.c_str());
        
        // Parse AndroidManifest.xml
        parseManifest(apkPath);
        
        // Parse resources.arsc
        parseResources(apkPath);
        
        // Build view hierarchy from main activity layout
        buildViewHierarchy();
        
        return true;
    }
    
    // Create native NSViews from Android Views
    void createNativeUI(NSView* container) {
        NSLog(@"[AndroidUI] Creating native UI with %zu views", views_.size());
        
        // Clear existing
        for (NSView* subview in [container subviews]) {
            [subview removeFromSuperview];
        }
        
        // Create views
        for (const auto& view : views_) {
            createNativeView(view, container);
        }
        
        NSLog(@"[AndroidUI] Native UI created");
    }
    
    // Update render (called each frame)
    void update() {
        // Animate or update UI state
    }
    
    std::string getAppName() const { return appName_; }
    std::string getPackageName() const { return packageName_; }

private:
    std::string appName_ = "Android App";
    std::string packageName_ = "com.example";
    std::vector<AndroidView> views_;
    
    void parseManifest(const std::string& apkPath) {
        // Extract package name from last directory component
        // Path format: .../apps/installed/com.termux
        std::string path = apkPath;
        if (!path.empty() && path.back() == '/') {
            path.pop_back();  // Remove trailing slash
        }
        
        size_t lastSlash = path.rfind('/');
        if (lastSlash != std::string::npos) {
            std::string dirName = path.substr(lastSlash + 1);
            // Validate it looks like a package name (has dots)
            if (dirName.find('.') != std::string::npos && dirName.length() > 3) {
                packageName_ = dirName;
                appName_ = dirName;
            }
        }
        
        NSLog(@"[AndroidUI] Package from path: %s", packageName_.c_str());
    }
    
    void parseResources(const std::string& apkPath) {
        // TODO: Parse resources.arsc for strings, colors, layouts
        NSLog(@"[AndroidUI] Resources parsed (stub)");
    }
    
    void buildViewHierarchy() {
        // Create a sample Android-like UI based on app type
        // This would normally parse layout XML from APK
        
        if (packageName_.find("termux") != std::string::npos) {
            // Termux-style terminal UI
            buildTermuxUI();
        } else if (packageName_.find("game") != std::string::npos || 
                   packageName_.find("roblox") != std::string::npos) {
            // Game UI - mostly fullscreen surface
            buildGameUI();
        } else {
            // Generic app UI
            buildGenericUI();
        }
    }
    
    void buildTermuxUI() {
        // Termux terminal emulator UI - fullscreen dark terminal, BLANK
        AndroidView root;
        root.type = AndroidViewType::FrameLayout;
        root.x = 0; root.y = 0;
        root.width = 800; root.height = 600;
        root.backgroundColor = 0xFF0D0D0D; // Termux black background
        
        // Terminal content area - BLANK, real app will write here
        AndroidView terminal;
        terminal.type = AndroidViewType::TextView;
        terminal.text = "";  // NO FAKE TEXT
        terminal.x = 8; terminal.y = 8;
        terminal.width = 784; terminal.height = 584;
        terminal.textColor = 0xFFCCCCCC;
        terminal.textSize = 12;
        
        root.children.push_back(terminal);
        views_.push_back(root);
        
        NSLog(@"[AndroidUI] Termux terminal UI built (blank - waiting for real terminal output)");
    }
    
    void buildGameUI() {
        // Game surface - mostly just background for GL rendering
        AndroidView surface;
        surface.type = AndroidViewType::FrameLayout;
        surface.x = 0; surface.y = 0;
        surface.width = 800; surface.height = 600;
        surface.backgroundColor = 0xFF000000; // Black for game
        
        views_.push_back(surface);
        
        NSLog(@"[AndroidUI] Game UI built");
    }
    
    void buildGenericUI() {
        // Generic Android app UI with toolbar and content
        AndroidView root;
        root.type = AndroidViewType::LinearLayout;
        root.x = 0; root.y = 0;
        root.width = 800; root.height = 600;
        root.backgroundColor = 0xFFF5F5F5; // Light gray bg
        
        // Action bar
        AndroidView actionBar;
        actionBar.type = AndroidViewType::FrameLayout;
        actionBar.x = 0; actionBar.y = 0;
        actionBar.width = 800; actionBar.height = 56;
        actionBar.backgroundColor = 0xFF6200EE; // Material purple
        
        AndroidView title;
        title.type = AndroidViewType::TextView;
        title.text = appName_;
        title.x = 16; title.y = 18;
        title.width = 300; title.height = 24;
        title.textColor = 0xFFFFFFFF;
        title.textSize = 20;
        
        actionBar.children.push_back(title);
        root.children.push_back(actionBar);
        
        // Content area - BLANK, will be filled by actual app execution
        AndroidView content;
        content.type = AndroidViewType::TextView;
        content.text = "";  // NO FAKE TEXT - App must populate
        content.x = 16; content.y = 72;
        content.width = 768; content.height = 400;
        content.textColor = 0xFF333333;
        content.textSize = 16;
        
        root.children.push_back(content);
        views_.push_back(root);
        
        NSLog(@"[AndroidUI] Generic UI built (blank - waiting for app content)");
    }
    
    void createNativeView(const AndroidView& view, NSView* parent) {
        @autoreleasepool {
            NSRect frame = NSMakeRect(view.x, 
                                      parent.frame.size.height - view.y - view.height,
                                      view.width, view.height);
            
            switch (view.type) {
                case AndroidViewType::FrameLayout:
                case AndroidViewType::LinearLayout:
                case AndroidViewType::RelativeLayout: {
                    NSView* container = [[NSView alloc] initWithFrame:frame];
                    [container setWantsLayer:YES];
                    container.layer.backgroundColor = 
                        CGColorCreateGenericRGB(
                            ((view.backgroundColor >> 16) & 0xFF) / 255.0,
                            ((view.backgroundColor >> 8) & 0xFF) / 255.0,
                            (view.backgroundColor & 0xFF) / 255.0,
                            ((view.backgroundColor >> 24) & 0xFF) / 255.0);
                    
                    for (const auto& child : view.children) {
                        createNativeView(child, container);
                    }
                    
                    [parent addSubview:container];
                    break;
                }
                    
                case AndroidViewType::TextView: {
                    NSTextField* textField = [[NSTextField alloc] initWithFrame:frame];
                    [textField setStringValue:@(view.text.c_str())];
                    [textField setBezeled:NO];
                    [textField setDrawsBackground:NO];
                    [textField setEditable:NO];
                    [textField setSelectable:NO];
                    
                    NSColor* textColor = [NSColor colorWithRed:((view.textColor >> 16) & 0xFF) / 255.0
                                                        green:((view.textColor >> 8) & 0xFF) / 255.0
                                                         blue:(view.textColor & 0xFF) / 255.0
                                                        alpha:((view.textColor >> 24) & 0xFF) / 255.0];
                    [textField setTextColor:textColor];
                    
                    NSFont* font = [NSFont systemFontOfSize:view.textSize];
                    [textField setFont:font];
                    
                    [parent addSubview:textField];
                    break;
                }
                    
                case AndroidViewType::Button: {
                    NSButton* button = [[NSButton alloc] initWithFrame:frame];
                    [button setTitle:@(view.text.c_str())];
                    [button setBezelStyle:NSBezelStyleRounded];
                    
                    NSColor* bgColor = [NSColor colorWithRed:((view.backgroundColor >> 16) & 0xFF) / 255.0
                                                       green:((view.backgroundColor >> 8) & 0xFF) / 255.0
                                                        blue:(view.backgroundColor & 0xFF) / 255.0
                                                       alpha:((view.backgroundColor >> 24) & 0xFF) / 255.0];
                    [button setContentTintColor:bgColor];
                    
                    [parent addSubview:button];
                    break;
                }
                    
                default:
                    break;
            }
        }
    }
};

} // namespace runtime
} // namespace apkx

// C interface
extern "C" {

void* android_ui_create(const char* apkPath) {
    using namespace apkx::runtime;
    auto* renderer = new AndroidUIRenderer();
    renderer->loadFromAPK(apkPath);
    return renderer;
}

void android_ui_build(void* handle, void* nativeView) {
    using namespace apkx::runtime;
    auto* renderer = static_cast<AndroidUIRenderer*>(handle);
    renderer->createNativeUI((__bridge NSView*)nativeView);
}

void android_ui_destroy(void* handle) {
    using namespace apkx::runtime;
    delete static_cast<AndroidUIRenderer*>(handle);
}

const char* android_ui_get_title(void* handle) {
    using namespace apkx::runtime;
    auto* renderer = static_cast<AndroidUIRenderer*>(handle);
    static std::string title;
    title = renderer->getAppName();
    return title.c_str();
}

}
