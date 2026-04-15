#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

// Prevent conflicts with any max/min macros
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

// Forward declare Metal types to avoid including Metal headers
namespace MTL {
    class Device;
    class CommandQueue;
    class Buffer;
    class Texture;
    class RenderPipelineState;
    class Function;
    class Library;
}

namespace apkx {
namespace runtime {

// OpenGL ES → Metal translation layer
// This is critical for running Android games/graphics apps

// GLES 2.0 types
typedef uint32_t GLenum;
typedef uint32_t GLuint;
typedef int32_t GLint;
typedef int32_t GLsizei;
typedef int64_t GLintptr;     // Pointer-sized offset
typedef int64_t GLsizeiptr;  // Pointer-sized size
typedef float GLfloat;
typedef void GLvoid;
typedef char GLchar;
typedef uint8_t GLboolean;
typedef uint32_t GLbitfield;
typedef const void* GLvoidPointer;

// GLES constants
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;

// Shader program
struct ShaderProgram {
    GLuint id;
    std::string vertex_source;
    std::string fragment_source;
    
    // Metal compiled shaders
    void* metal_vertex_function = nullptr;
    void* metal_fragment_function = nullptr;
    void* metal_pipeline = nullptr;
    
    std::map<std::string, GLint> attrib_locations;
    std::map<std::string, GLint> uniform_locations;
};

// Buffer object
struct BufferObject {
    GLuint id;
    GLenum target;
    size_t size;
    std::vector<uint8_t> data;
    void* metal_buffer = nullptr;
};

// Texture
struct Texture {
    GLuint id;
    GLenum target;
    GLsizei width;
    GLsizei height;
    void* metal_texture = nullptr;
};

// GLSL → Metal Shading Language translator
class GLSLTranslator {
public:
    GLSLTranslator();
    
    // Translate vertex shader
    std::string translateVertex(const std::string& glsl);
    
    // Translate fragment shader
    std::string translateFragment(const std::string& glsl);
    
    // Extract attribute/uniform info
    std::vector<std::string> extractAttributes(const std::string& glsl);
    std::vector<std::string> extractUniforms(const std::string& glsl);

private:
    // GLSL parser helpers
    std::string replacePrecisionQualifiers(const std::string& glsl);
    std::string replaceBuiltins(const std::string& glsl);
    std::string addMetalHeaders(const std::string& msl);
};

// GLES → Metal API translation
class MetalGraphicsContext {
public:
    MetalGraphicsContext();
    ~MetalGraphicsContext();
    
    bool initialize(void* native_view);
    bool isReady() const { return ready_; }
    
    // Buffer operations
    void glGenBuffers(GLsizei n, GLuint* buffers);
    void glDeleteBuffers(GLsizei n, const GLuint* buffers);
    void glBindBuffer(GLenum target, GLuint buffer);
    void glBufferData(GLenum target, GLsizeiptr size, 
                      const GLvoid* data, GLenum usage);
    void glBufferSubData(GLenum target, GLintptr offset, 
                         GLsizeiptr size, const GLvoid* data);
    
    // Shader operations
    GLuint glCreateShader(GLenum type);
    void glShaderSource(GLuint shader, GLsizei count, 
                        const GLchar** string, const GLint* length);
    void glCompileShader(GLuint shader);
    void glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
    GLuint glCreateProgram();
    void glAttachShader(GLuint program, GLuint shader);
    void glLinkProgram(GLuint program);
    void glUseProgram(GLuint program);
    GLint glGetAttribLocation(GLuint program, const GLchar* name);
    GLint glGetUniformLocation(GLuint program, const GLchar* name);
    void glUniform1f(GLint location, GLfloat v0);
    void glUniformMatrix4fv(GLint location, GLsizei count, 
                            GLboolean transpose, const GLfloat* value);
    
    // Vertex arrays
    void glEnableVertexAttribArray(GLuint index);
    void glDisableVertexAttribArray(GLuint index);
    void glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                               GLboolean normalized, GLsizei stride,
                               const GLvoid* pointer);
    
    // Drawing
    void glDrawArrays(GLenum mode, GLint first, GLsizei count);
    void glDrawElements(GLenum mode, GLsizei count, GLenum type,
                        const GLvoid* indices);
    
    // Textures
    void glGenTextures(GLsizei n, GLuint* textures);
    void glBindTexture(GLenum target, GLuint texture);
    void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                      GLsizei width, GLsizei height, GLint border,
                      GLenum format, GLenum type, const GLvoid* pixels);
    void glTexParameteri(GLenum target, GLenum pname, GLint param);
    
    // State
    void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
    void glClearColor(GLfloat red, GLfloat green, 
                      GLfloat blue, GLfloat alpha);
    void glClear(GLbitfield mask);
    void glEnable(GLenum cap);
    void glDisable(GLenum cap);
    void glBlendFunc(GLenum sfactor, GLenum dfactor);
    
    // Present (swap buffers)
    void present();
    
    // Debug
    void dumpState();
    
    // State control
    void setReady(bool ready) { ready_ = ready; }

private:
    bool ready_ = false;
    
    // Metal objects
    void* metal_device_ = nullptr;
    void* metal_command_queue_ = nullptr;
    void* metal_layer_ = nullptr;
    
    // State tracking
    GLuint current_program_ = 0;
    GLuint current_buffer_ = 0;
    GLuint current_texture_ = 0;
    GLuint next_id_ = 1;
    
    std::map<GLuint, std::unique_ptr<ShaderProgram>> programs_;
    std::map<GLuint, std::unique_ptr<BufferObject>> buffers_;
    std::map<GLuint, std::unique_ptr<Texture>> textures_;
    
    GLSLTranslator shader_translator_;
    
    bool initializeMetal();
    void* compileShader(const std::string& msl, bool vertex);
};

// Global graphics context
MetalGraphicsContext* getGraphicsContext();

} // namespace runtime
} // namespace apkx
