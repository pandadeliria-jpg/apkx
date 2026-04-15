// macOS-native graphics backend using Cocoa + Metal
// This creates actual windows and renders graphics

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include "apkx/graphics_metal.hpp"
#include <iostream>
#include <map>

// Global window close flag - declared early and defined later
extern "C" bool g_window_closed_flag = false;

// Objective-C++ bridge class
@interface ApkxMetalView : MTKView
@property (nonatomic, assign) apkx::runtime::MetalGraphicsContext* graphicsContext;
@end

@implementation ApkxMetalView

- (void)drawRect:(NSRect)rect {
    if (self.graphicsContext) {
        // Trigger frame render
    }
}

@end

// Window delegate to track close events
@interface ApkxWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) BOOL* windowClosedFlag;
@end

@implementation ApkxWindowDelegate

- (void)windowWillClose:(NSNotification *)notification {
    if (self.windowClosedFlag) {
        *self.windowClosedFlag = YES;
        NSLog(@"[*] Window close detected");
    }
}

@end

@interface ApkxWindowController : NSWindowController
@property (nonatomic, strong) ApkxMetalView* metalView;
@property (nonatomic, assign) apkx::runtime::MetalGraphicsContext* graphicsContext;
- (void)createWindowWithTitle:(NSString*)title width:(int)width height:(int)height;
- (void)runFrame;
@end

@implementation ApkxWindowController

- (void)createWindowWithTitle:(NSString*)title width:(int)width height:(int)height {
    // Create window
    NSRect frame = NSMakeRect(0, 0, width, height);
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                    styleMask:(NSWindowStyleMaskTitled |
                                                               NSWindowStyleMaskClosable |
                                                               NSWindowStyleMaskMiniaturizable |
                                                               NSWindowStyleMaskResizable)
                                                      backing:NSBackingStoreBuffered
                                                        defer:NO];
    
    [window setTitle:title];
    [window center];
    
    // Create Metal device
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        NSLog(@"[!] Metal not supported");
        return;
    }
    
    NSLog(@"[*] Metal device: %@", [device name]);
    
    // Create Metal view
    self.metalView = [[ApkxMetalView alloc] initWithFrame:frame device:device];
    self.metalView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    self.metalView.preferredFramesPerSecond = 60;
    self.metalView.enableSetNeedsDisplay = YES;
    self.metalView.graphicsContext = self.graphicsContext;
    
    [window setContentView:self.metalView];
    
    self.window = window;
    
    // Set up delegate to detect window close
    ApkxWindowDelegate* delegate = [[ApkxWindowDelegate alloc] init];
    // Get the backend to set the flag - we'll use a global for now
    extern bool g_window_closed_flag;
    delegate.windowClosedFlag = &g_window_closed_flag;
    [window setDelegate:delegate];
    
    [self.window makeKeyAndOrderFront:nil];
    
    NSLog(@"[+] Window created: %dx%d", width, height);
}

- (void)runFrame {
    // Process events
    @autoreleasepool {
        NSEvent* event;
        while ((event = [self.window nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:[NSDate distantPast]
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES])) {
            [NSApp sendEvent:event];
        }
    }
    
    // Trigger draw
    [self.metalView setNeedsDisplay:YES];
}

@end

namespace apkx {
namespace runtime {

// Real Metal implementation
class MetalBackend {
public:
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    id<MTLLibrary> currentLibrary = nil;
    id<MTLRenderPipelineState> pipelineState = nil;
    id<MTLBuffer> vertexBuffer = nil;
    id<MTLBuffer> uniformBuffer = nil;
    
    ApkxWindowController* windowController = nil;
    MTKView* metalView = nil;
    bool windowClosed = false;
    
    // Render state
    float clearColor[4] = {0.0, 0.0, 0.0, 1.0};
    
    bool isWindowClosed() { return windowClosed; }
    
    bool initialize() {
        @autoreleasepool {
            device = MTLCreateSystemDefaultDevice();
            if (!device) {
                std::cerr << "[!] Metal not available\n";
                return false;
            }
            
            commandQueue = [device newCommandQueue];
            
            // Create default uniform buffer (4x4 matrix + extras)
            uniformBuffer = [device newBufferWithLength:256
                                               options:MTLResourceStorageModeShared];
            
            NSLog(@"[*] Metal backend initialized");
            NSLog(@"    Device: %@", [device name]);
            NSLog(@"    Supports ray tracing: %@", 
                  [device supportsRaytracing] ? @"YES" : @"NO");
            
            return true;
        }
    }
    
    void createWindow(const char* title, int width, int height) {
        @autoreleasepool {
            windowController = [[ApkxWindowController alloc] init];
            [windowController createWindowWithTitle:@(title) 
                                              width:width 
                                             height:height];
            metalView = ((ApkxWindowController*)windowController).metalView;
        }
    }
    
    void present() {
        if (!metalView) return;
        
        @autoreleasepool {
            // Process window events to prevent "not responding"
            NSEvent* event;
            while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate distantPast]
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES])) {
                [NSApp sendEvent:event];
            }
            
            // Get CAMetalLayer from the view
            CAMetalLayer* metalLayer = (CAMetalLayer*)metalView.layer;
            if (!metalLayer) return;
            
            // Get next drawable
            id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
            if (!drawable) return;
            
            // Create command buffer
            id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
            
            // Create render pass descriptor
            MTLRenderPassDescriptor* passDescriptor = 
                [MTLRenderPassDescriptor renderPassDescriptor];
            passDescriptor.colorAttachments[0].texture = drawable.texture;
            passDescriptor.colorAttachments[0].clearColor = 
                MTLClearColorMake(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
            passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
            passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
            
            // Create render encoder
            id<MTLRenderCommandEncoder> encoder = 
                [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
            
            // Set pipeline state if available
            if (pipelineState) {
                [encoder setRenderPipelineState:pipelineState];
                
                // Set vertex buffer if available
                if (vertexBuffer) {
                    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
                }
                
                // Set uniform buffer
                if (uniformBuffer) {
                    [encoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
                    [encoder setFragmentBuffer:uniformBuffer offset:0 atIndex:0];
                }
                
                // Draw (dummy triangle for now)
                if (vertexBuffer) {
                    [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                                vertexStart:0
                                vertexCount:3];
                }
            }
            
            [encoder endEncoding];
            
            // Present and commit
            [commandBuffer presentDrawable:drawable];
            [commandBuffer commit];
        }
    }
    
    id<MTLLibrary> compileShaderLibrary(const std::string& source) {
        @autoreleasepool {
            NSError* error = nil;
            
            NSString* sourceString = @(source.c_str());
            MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
            options.languageVersion = MTLLanguageVersion2_4;
            
            id<MTLLibrary> library = [device newLibraryWithSource:sourceString
                                                          options:options
                                                            error:&error];
            
            if (error) {
                NSLog(@"[!] Shader compile error: %@", error);
                return nil;
            }
            
            currentLibrary = library;
            return library;
        }
    }
    
    bool createPipelineState(id<MTLFunction> vertexFunc, id<MTLFunction> fragmentFunc) {
        @autoreleasepool {
            MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
            descriptor.vertexFunction = vertexFunc;
            descriptor.fragmentFunction = fragmentFunc;
            descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            
            // Set up vertex descriptor
            MTLVertexDescriptor* vertexDescriptor = [[MTLVertexDescriptor alloc] init];
            vertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;
            vertexDescriptor.attributes[0].offset = 0;
            vertexDescriptor.attributes[0].bufferIndex = 0;
            vertexDescriptor.attributes[1].format = MTLVertexFormatFloat4;
            vertexDescriptor.attributes[1].offset = 16;
            vertexDescriptor.attributes[1].bufferIndex = 0;
            vertexDescriptor.layouts[0].stride = 32;
            vertexDescriptor.layouts[0].stepRate = 1;
            vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
            descriptor.vertexDescriptor = vertexDescriptor;
            
            NSError* error = nil;
            pipelineState = [device newRenderPipelineStateWithDescriptor:descriptor
                                                                    error:&error];
            
            if (error) {
                NSLog(@"[!] Pipeline creation failed: %@", error);
                return false;
            }
            
            NSLog(@"[+] Pipeline state created");
            return true;
        }
    }
    
    void setClearColor(float r, float g, float b, float a) {
        clearColor[0] = r;
        clearColor[1] = g;
        clearColor[2] = b;
        clearColor[3] = a;
    }
    
    id<MTLBuffer> createVertexBuffer(const void* data, size_t size) {
        @autoreleasepool {
            id<MTLBuffer> buffer = [device newBufferWithBytes:data
                                                      length:size
                                                     options:MTLResourceStorageModeShared];
            vertexBuffer = buffer;
            return buffer;
        }
    }
};

// Global backend instance
static std::unique_ptr<MetalBackend> g_backend;

// Getter for C functions - needs extern C to avoid name mangling
extern "C" MetalBackend* getMetalBackend() { return g_backend.get(); }
extern "C" bool get_window_closed() { return g_window_closed_flag; }

// Extended graphics context with real rendering
class RealGraphicsContext : public MetalGraphicsContext {
public:
    bool realInitialize() {
        g_backend = std::make_unique<MetalBackend>();
        if (!g_backend->initialize()) {
            return false;
        }
        
        // Create default window
        g_backend->createWindow("Android App on macOS", 800, 600);
        
        setReady(true);
        return true;
    }
    
    void realPresent() {
        if (g_backend && g_backend->windowController) {
            g_backend->present();
        }
    }
};

// Override to provide real implementation
MetalGraphicsContext* getGraphicsContext() {
    static std::unique_ptr<RealGraphicsContext> g_realGraphics;
    if (!g_realGraphics) {
        g_realGraphics = std::make_unique<RealGraphicsContext>();
    }
    return g_realGraphics.get();
}

} // namespace runtime
} // namespace apkx

// Android UI renderer interface (forward declarations)
extern "C" void* android_ui_create(const char* apkPath);
extern "C" void android_ui_build(void* handle, void* nativeView);
extern "C" void android_ui_destroy(void* handle);
extern "C" const char* android_ui_get_title(void* handle);

// Forward declaration for MetalBackend getter (defined in namespace above)
extern "C" apkx::runtime::MetalBackend* getMetalBackend();

static void* g_ui_renderer = nullptr;

// C interface for integration
extern "C" {

int graphics_initialize(const char* title, int width, int height);
int graphics_initialize_with_apk(const char* title, int width, int height, const char* apk_path);
void graphics_present();
void graphics_set_clear_color(float r, float g, float b, float a);

int graphics_initialize(const char* title, int width, int height) {
    return graphics_initialize_with_apk(title, width, height, nullptr);
}

} // extern "C"

extern "C" int graphics_initialize_with_apk(const char* title, int width, int height, const char* apk_path) {
    using namespace apkx::runtime;
    
    @autoreleasepool {
        // Initialize Cocoa application
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        // Create graphics context
        RealGraphicsContext* ctx = static_cast<RealGraphicsContext*>(getGraphicsContext());
        if (!ctx->realInitialize()) {
            return 0;
        }
        
        // Get backend for window access
        MetalBackend* backend = getMetalBackend();
        
        // Create Android UI renderer if APK provided
        if (apk_path && backend) {
            g_ui_renderer = android_ui_create(apk_path);
            if (g_ui_renderer) {
                const char* app_title = android_ui_get_title(g_ui_renderer);
                if (app_title && strlen(app_title) > 0) {
                    title = app_title;
                }
                
                // Build native UI on the window's content view
                NSWindow* window = backend->windowController.window;
                if (window) {
                    android_ui_build(g_ui_renderer, (__bridge void*)window.contentView);
                }
            }
        }
        
        // Set clear color and mark ready
        backend->setClearColor(0.1, 0.1, 0.1, 1.0);
        ctx->setReady(true);
        
        NSLog(@"[+] Graphics initialized: %dx%d (Android UI only, no test geometry)", width, height);
        return 1;
    }
}

extern "C" void graphics_present() {
    using namespace apkx::runtime;
    RealGraphicsContext* ctx = static_cast<RealGraphicsContext*>(getGraphicsContext());
    ctx->realPresent();
}

extern "C" void graphics_set_clear_color(float r, float g, float b, float a) {
    auto* backend = apkx::runtime::getMetalBackend();
    if (backend) {
        backend->setClearColor(r, g, b, a);
    }
}

void graphics_run_loop() {
    @autoreleasepool {
        [NSApp run];
    }
}
