#include "apkx/graphics_metal.hpp"

#include <iostream>
#include <cstring>

namespace apkx {
namespace runtime {

// GLSLTranslator implementation
GLSLTranslator::GLSLTranslator() = default;

std::string GLSLTranslator::translateVertex(const std::string& glsl) {
    std::string msl = glsl;
    
    // Replace precision qualifiers
    msl = replacePrecisionQualifiers(msl);
    
    // Replace gl_Position with Metal equivalent
    size_t pos;
    while ((pos = msl.find("gl_Position")) != std::string::npos) {
        msl.replace(pos, 11, "float4 gl_Position [[position]]");
    }
    
    // Replace attribute/varying
    while ((pos = msl.find("attribute")) != std::string::npos) {
        msl.replace(pos, 9, "vertex_attribute");
    }
    while ((pos = msl.find("varying")) != std::string::npos) {
        msl.replace(pos, 7, "vertex_out");
    }
    
    return addMetalHeaders(msl);
}

std::string GLSLTranslator::translateFragment(const std::string& glsl) {
    std::string msl = glsl;
    
    msl = replacePrecisionQualifiers(msl);
    
    // Replace gl_FragColor
    size_t pos;
    while ((pos = msl.find("gl_FragColor")) != std::string::npos) {
        msl.replace(pos, 12, "float4 frag_color [[color(0)]]");
    }
    
    return addMetalHeaders(msl);
}

std::vector<std::string> GLSLTranslator::extractAttributes(
    const std::string& glsl) {
    std::vector<std::string> attrs;
    size_t pos = 0;
    while ((pos = glsl.find("attribute ", pos)) != std::string::npos) {
        size_t end = glsl.find(';', pos);
        if (end != std::string::npos) {
            attrs.push_back(glsl.substr(pos, end - pos));
        }
        pos = end;
    }
    return attrs;
}

std::vector<std::string> GLSLTranslator::extractUniforms(
    const std::string& glsl) {
    std::vector<std::string> uniforms;
    size_t pos = 0;
    while ((pos = glsl.find("uniform ", pos)) != std::string::npos) {
        size_t end = glsl.find(';', pos);
        if (end != std::string::npos) {
            uniforms.push_back(glsl.substr(pos, end - pos));
        }
        pos = end;
    }
    return uniforms;
}

std::string GLSLTranslator::replacePrecisionQualifiers(
    const std::string& glsl) {
    std::string result = glsl;
    // Remove precision statements
    size_t pos;
    while ((pos = result.find("precision ")) != std::string::npos) {
        size_t end = result.find(';', pos);
        if (end != std::string::npos) {
            result.erase(pos, end - pos + 1);
        }
    }
    return result;
}

std::string GLSLTranslator::replaceBuiltins(const std::string& glsl) {
    // Replace vec2/vec3/vec4 with float2/float3/float4
    std::string result = glsl;
    // This is a simplified replacement
    return result;
}

std::string GLSLTranslator::addMetalHeaders(const std::string& msl) {
    return "#include <metal_stdlib>\n"
           "using namespace metal;\n\n"
           + msl;
}

// MetalGraphicsContext implementation
MetalGraphicsContext::MetalGraphicsContext() = default;

MetalGraphicsContext::~MetalGraphicsContext() {
    // Cleanup Metal resources
}

bool MetalGraphicsContext::initialize(void* native_view) {
    std::cout << "[*] MetalGraphicsContext initializing...\n";
    
    // TODO: Initialize Metal device, command queue
    // This requires actual Metal framework calls
    
    ready_ = false;  // Not yet implemented
    std::cout << "[!] Metal graphics not yet fully implemented\n";
    
    return ready_;
}

void MetalGraphicsContext::glGenBuffers(GLsizei n, GLuint* buffers) {
    for (GLsizei i = 0; i < n; i++) {
        GLuint id = next_id_++;
        buffers[i] = id;
        buffers_[id] = std::make_unique<BufferObject>();
        buffers_[id]->id = id;
    }
}

void MetalGraphicsContext::glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    for (GLsizei i = 0; i < n; i++) {
        buffers_.erase(buffers[i]);
    }
}

void MetalGraphicsContext::glBindBuffer(GLenum target, GLuint buffer) {
    current_buffer_ = buffer;
}

void MetalGraphicsContext::glBufferData(GLenum target, GLsizeiptr size,
                                        const GLvoid* data, GLenum usage) {
    auto it = buffers_.find(current_buffer_);
    if (it != buffers_.end()) {
        it->second->size = size;
        it->second->data.resize(size);
        if (data) {
            std::memcpy(it->second->data.data(), data, size);
        }
        it->second->target = target;
    }
}

GLuint MetalGraphicsContext::glCreateShader(GLenum type) {
    GLuint id = next_id_++;
    auto prog = std::make_unique<ShaderProgram>();
    prog->id = id;
    programs_[id] = std::move(prog);
    return id;
}

void MetalGraphicsContext::glShaderSource(GLuint shader, GLsizei count,
                                          const GLchar** string, 
                                          const GLint* length) {
    // Concatenate shader sources
    std::string source;
    for (GLsizei i = 0; i < count; i++) {
        if (length && length[i] > 0) {
            source.append(string[i], length[i]);
        } else {
            source.append(string[i]);
        }
    }
    
    auto it = programs_.find(shader);
    if (it != programs_.end()) {
        // Detect shader type from source
        if (source.find("gl_Position") != std::string::npos) {
            it->second->vertex_source = source;
        } else {
            it->second->fragment_source = source;
        }
    }
}

void MetalGraphicsContext::glCompileShader(GLuint shader) {
    auto it = programs_.find(shader);
    if (it == programs_.end()) return;
    
    // Translate to Metal
    if (!it->second->vertex_source.empty()) {
        std::string msl = shader_translator_.translateVertex(
            it->second->vertex_source);
        // TODO: Compile MSL to Metal function
        std::cout << "[GL→Metal] Vertex shader translated\n";
    }
    
    if (!it->second->fragment_source.empty()) {
        std::string msl = shader_translator_.translateFragment(
            it->second->fragment_source);
        std::cout << "[GL→Metal] Fragment shader translated\n";
    }
}

GLuint MetalGraphicsContext::glCreateProgram() {
    GLuint id = next_id_++;
    auto prog = std::make_unique<ShaderProgram>();
    prog->id = id;
    programs_[id] = std::move(prog);
    return id;
}

void MetalGraphicsContext::glAttachShader(GLuint program, GLuint shader) {
    // Shaders are already tracked in programs_ map
}

void MetalGraphicsContext::glLinkProgram(GLuint program) {
    // TODO: Create Metal pipeline state
    std::cout << "[GL→Metal] Program " << program << " linked\n";
}

void MetalGraphicsContext::glUseProgram(GLuint program) {
    current_program_ = program;
}

void MetalGraphicsContext::glViewport(GLint x, GLint y, 
                                      GLsizei width, GLsizei height) {
    // TODO: Set Metal viewport
    std::cout << "[GL→Metal] Viewport: " << x << "," << y << " " 
              << width << "x" << height << "\n";
}

void MetalGraphicsContext::glClearColor(GLfloat red, GLfloat green,
                                        GLfloat blue, GLfloat alpha) {
    // TODO: Set clear color in Metal
}

void MetalGraphicsContext::glClear(GLbitfield mask) {
    // TODO: Clear Metal render target
}

void MetalGraphicsContext::glDrawArrays(GLenum mode, GLint first, 
                                       GLsizei count) {
    if (!ready_) {
        std::cerr << "[!] Metal not initialized, skipping draw\n";
        return;
    }
    
    // TODO: Encode Metal draw call
    std::cout << "[GL→Metal] DrawArrays mode=" << mode 
              << " first=" << first << " count=" << count << "\n";
}

void MetalGraphicsContext::present() {
    // TODO: Commit Metal command buffer
}

void MetalGraphicsContext::dumpState() {
    std::cout << "=== Graphics State ===\n";
    std::cout << "Current program: " << current_program_ << "\n";
    std::cout << "Current buffer: " << current_buffer_ << "\n";
    std::cout << "Programs: " << programs_.size() << "\n";
    std::cout << "Buffers: " << buffers_.size() << "\n";
    std::cout << "Textures: " << textures_.size() << "\n";
}

// Global graphics context defined in graphics_cocoa.mm
extern MetalGraphicsContext* getGraphicsContext();

} // namespace runtime
} // namespace apkx
