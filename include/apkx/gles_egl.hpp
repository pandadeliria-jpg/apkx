#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <iostream>

namespace apkx {
namespace runtime {

// GLES types
typedef uint32_t GLenum;
typedef uint32_t GLuint;
typedef int32_t GLint;
typedef int32_t GLsizei;
typedef uint8_t GLboolean;
typedef uint32_t GLbitfield;
typedef float GLfloat;

// EGL types (simplified)
typedef int32_t EGLint;
typedef void* EGLDisplay;
typedef void* EGLContext;
typedef void* EGLSurface;
typedef void* EGLConfig;

// EGL constants
constexpr EGLint EGL_SUCCESS = 0x3000;
constexpr EGLint EGL_NOT_INITIALIZED = 0x3001;
constexpr EGLint EGL_BAD_ALLOC = 0x3003;
constexpr EGLint EGL_BAD_DISPLAY = 0x3008;
constexpr EGLint EGL_BAD_CONFIG = 0x3009;
constexpr EGLint EGL_BAD_SURFACE = 0x300B;
constexpr EGLint EGL_BAD_CONTEXT = 0x300C;
constexpr EGLint EGL_SURFACE_TYPE = 0x3040;
constexpr EGLint EGL_WINDOW_BIT = 0x0004;
constexpr EGLint EGL_RENDERABLE_TYPE = 0x3044;
constexpr EGLint EGL_OPENGL_ES_BIT = 0x0001;
constexpr EGLint EGL_RED_SIZE = 0x3024;
constexpr EGLint EGL_GREEN_SIZE = 0x3025;
constexpr EGLint EGL_BLUE_SIZE = 0x3026;
constexpr EGLint EGL_ALPHA_SIZE = 0x3022;
constexpr EGLint EGL_DEPTH_SIZE = 0x3028;
constexpr EGLint EGL_STENCIL_SIZE = 0x302A;
constexpr EGLint EGL_DEFAULT_DISPLAY = 0;

class EGLStub {
public:
    static EGLDisplay eglGetDisplay(EGLint display_id) {
        if (display_id == EGL_DEFAULT_DISPLAY) {
            return reinterpret_cast<EGLDisplay>(1);  // Return non-null
        }
        return EGLDisplay(0);
    }
    
    static EGLint eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
        if (major) *major = 1;
        if (minor) *minor = 4;
        std::cout << "[EGL] Initialized\n";
        return EGL_SUCCESS;
    }
    
    static EGLint eglTerminate(EGLDisplay dpy) {
        return EGL_SUCCESS;
    }
    
    static EGLint eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list,
                                    EGLConfig* configs, EGLint config_size,
                                    EGLint* num_config) {
        if (configs && config_size > 0) {
            configs[0] = reinterpret_cast<EGLConfig>(1);
        }
        if (num_config) *num_config = 1;
        return EGL_SUCCESS;
    }
    
    static EGLint eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                                       EGLint attribute, EGLint* value) {
        if (value) *value = 8;
        return EGL_SUCCESS;
    }
    
    static EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                               void* native_window, const EGLint* attrib_list) {
        return reinterpret_cast<EGLSurface>(1);
    }
    
    static EGLint eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
        return EGL_SUCCESS;
    }
    
    static EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
                                        EGLContext share_context, const EGLint* attrib_list) {
        return reinterpret_cast<EGLContext>(1);
    }
    
    static EGLint eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
        return EGL_SUCCESS;
    }
    
    static EGLint eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, 
                                  EGLSurface read, EGLContext ctx) {
        std::cout << "[EGL] MakeCurrent\n";
        return EGL_SUCCESS;
    }
    
    static EGLint eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
        // Would present to Metal
        return EGL_SUCCESS;
    }
    
    static EGLint eglQueryString(EGLDisplay dpy, EGLint name) {
        if (name == 0x3053) { // EGL_VENDOR
            return static_cast<EGLint>(reinterpret_cast<intptr_t>("Android Compat Layer"));
        }
        return 0;
    }
};

// GLES 2.0 function stubs
class GLESStub {
public:
    static void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
        std::cout << "[GLES] Viewport " << width << "x" << height << "\n";
    }
    
    static void glClear(GLbitfield mask) {
        // Clear buffers
    }
    
    static void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
        std::cout << "[GLES] ClearColor " << red << "," << green << "," << blue << "," << alpha << "\n";
    }
    
    static void glEnable(GLenum cap) {
        std::cout << "[GLES] Enable " << std::hex << cap << std::dec << "\n";
    }
    
    static void glDisable(GLenum cap) {
        std::cout << "[GLES] Disable " << std::hex << cap << std::dec << "\n";
    }
    
    static void glGenTextures(GLsizei n, GLuint* textures) {
        for (GLsizei i = 0; i < n; i++) {
            textures[i] = i + 1;
        }
    }
    
    static void glBindTexture(GLenum target, GLuint texture) {
        std::cout << "[GLES] BindTexture " << texture << "\n";
    }
    
    static void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                             GLsizei width, GLsizei height, GLint border,
                             GLenum format, GLenum type, const void* pixels) {
        std::cout << "[GLES] TexImage2D " << width << "x" << height << "\n";
    }
    
    static void glTexParameteri(GLenum target, GLenum pname, GLint param) {
        // Set texture parameters
    }
    
    static void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
        std::cout << "[GLES] DrawArrays " << count << " vertices\n";
    }
    
    static void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
        std::cout << "[GLES] DrawElements " << count << " indices\n";
    }
    
    static GLuint glCreateProgram() {
        return 1;
    }
    
    static void glDeleteProgram(GLuint program) {}
    
    static void glUseProgram(GLuint program) {
        std::cout << "[GLES] UseProgram " << program << "\n";
    }
    
    static void glUniform1f(GLint location, GLfloat v0) {
        // Set uniform
    }
    
    static void glUniformMatrix4fv(GLint location, GLsizei count, 
                                     GLboolean transpose, const GLfloat* value) {
        // Set matrix uniform
    }
    
    static void glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                                       GLboolean normalized, GLsizei stride,
                                       const void* pointer) {
        // Set vertex attribute
    }
    
    static void glEnableVertexAttribArray(GLuint index) {
        // Enable attribute
    }
};

} // namespace runtime
} // namespace apkx