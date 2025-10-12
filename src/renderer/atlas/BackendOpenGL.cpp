#include "pch.h"
#include "BackendOpenGL.h"

#include <tuple>
#include <fstream>
#include <filesystem>

#ifndef GL_SHADER_BINARY_FORMAT_SPIR_V_ARB
#define GL_SHADER_BINARY_FORMAT_SPIR_V_ARB 0x9551
#endif

using namespace Microsoft::Console::Render::Atlas;

namespace
{
    constexpr f32x2 g_quadVertices[] = {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f },
    };

    constexpr u16 g_quadIndices[] = {
        0, 1, 2,
        2, 3, 0,
    };
}

BackendOpenGL::BackendOpenGL(const RenderingPayload& p)
{
    _createContext(p);
    _loadExtensions();
    _detectFeatures();
    _createBuffers();
    _createTextures(p);
    _compileShaders();
    _setupVertexAttributes();
    _setupBlendState();
}

BackendOpenGL::~BackendOpenGL()
{
    ReleaseResources();
}

void BackendOpenGL::ReleaseResources() noexcept
{
    if (_shaderProgram)
    {
        glDeleteProgram(_shaderProgram);
        _shaderProgram = 0;
    }

    if (_vertexBuffer)
    {
        glDeleteBuffers(1, &_vertexBuffer);
        _vertexBuffer = 0;
    }

    if (_indexBuffer)
    {
        glDeleteBuffers(1, &_indexBuffer);
        _indexBuffer = 0;
    }

    if (_instanceBuffer)
    {
        glDeleteBuffers(1, &_instanceBuffer);
        _instanceBuffer = 0;
    }

    if (_vao)
    {
        glDeleteVertexArrays(1, &_vao);
        _vao = 0;
    }
}

bool BackendOpenGL::RequiresContinuousRedraw() noexcept
{
    return _requiresContinuousRedraw;
}

static void* _getGlProc(const char* name) noexcept
{
#ifdef _WIN32
    return reinterpret_cast<void*>(wglGetProcAddress(name));
#else
    return reinterpret_cast<void*>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
#endif
}

void BackendOpenGL::_createBuffers()
{
    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);

    glGenBuffers(1, &_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_quadVertices), g_quadVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &_indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(g_quadIndices), g_quadIndices, GL_STATIC_DRAW);

    glGenBuffers(1, &_instanceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QuadInstance) * MaxInstances, nullptr, GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

GLuint BackendOpenGL::_compileShaderFromSpirv(GLenum stage, const std::wstring& spirvPath)
{
#if defined(GL_ARB_gl_spirv)
    std::ifstream file(spirvPath, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return 0;
    }

    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    if (!file.read(buffer.data(), size))
    {
        return 0;
    }

    GLuint shader = glCreateShader(stage);
    glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, buffer.data(), static_cast<GLsizei>(buffer.size()));

    using PFNGLSPECIALIZESHADERPROC = void(APIENTRYP)(GLuint, const GLchar*, GLuint, const GLuint*, const GLuint*);
    static PFNGLSPECIALIZESHADERPROC specializeShader = reinterpret_cast<PFNGLSPECIALIZESHADERPROC>(_getGlProc("glSpecializeShader"));
    if (specializeShader)
    {
        specializeShader(shader, "main", 0, nullptr, nullptr);
    }
    else
    {
        glDeleteShader(shader);
        shader = 0;
    }

    return shader;
#else
    (void)stage;
    (void)spirvPath;
    return 0;
#endif
}

GLuint BackendOpenGL::_compileShaderFromGlsl(GLenum stage, const std::wstring& glslPath)
{
    std::ifstream file(glslPath);
    if (!file)
    {
        return 0;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (source.empty())
    {
        return 0;
    }

    GLuint shader = glCreateShader(stage);
    const GLchar* srcPtr = source.c_str();
    glShaderSource(shader, 1, &srcPtr, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        glDeleteShader(shader);
        shader = 0;
    }

    return shader;
}

void BackendOpenGL::_compileShaders()
{
    std::filesystem::path shaderDir = std::filesystem::current_path() / L"build" / L"shaders" / L"UltraPerformance.GL";
    if (!std::filesystem::exists(shaderDir))
    {
        shaderDir = std::filesystem::path(L"build\\shaders\\UltraPerformance.GL");
    }

    _shaderSpirvVS = (shaderDir / L"shader_vs.spv").wstring();
    _shaderSpirvPS = (shaderDir / L"shader_ps.spv").wstring();
    _shaderGlslVS = (shaderDir / L"shader_vs.glsl").wstring();
    _shaderGlslPS = (shaderDir / L"shader_ps.glsl").wstring();

    GLuint vs = 0;
    GLuint fs = 0;

    if (_features.spirv_shaders && std::filesystem::exists(_shaderSpirvVS))
    {
        vs = _compileShaderFromSpirv(GL_VERTEX_SHADER, _shaderSpirvVS);
    }
    if (_features.spirv_shaders && std::filesystem::exists(_shaderSpirvPS))
    {
        fs = _compileShaderFromSpirv(GL_FRAGMENT_SHADER, _shaderSpirvPS);
    }

    if (!vs && std::filesystem::exists(_shaderGlslVS))
    {
        vs = _compileShaderFromGlsl(GL_VERTEX_SHADER, _shaderGlslVS);
    }
    if (!fs && std::filesystem::exists(_shaderGlslPS))
    {
        fs = _compileShaderFromGlsl(GL_FRAGMENT_SHADER, _shaderGlslPS);
    }

    if (vs && fs)
    {
        GLuint program = _linkProgram(vs, fs);
        if (program)
        {
            if (_shaderProgram)
            {
                glDeleteProgram(_shaderProgram);
            }
            _shaderProgram = program;
        }
    }

    if (!vs)
    {
        _shaderSpirvVS.clear();
    }
    if (!fs)
    {
        _shaderSpirvPS.clear();
    }
}

void BackendOpenGL::_setupVertexAttributes()
{
    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32x2), nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, _instanceBuffer);
    glVertexAttribPointer(1, 1, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(QuadInstance), reinterpret_cast<void*>(offsetof(QuadInstance, shadingType)));
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void BackendOpenGL::_setupBlendState()
{
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void BackendOpenGL::_loadExtensions()
{
    if (_getGlProc("glSpecializeShader"))
    {
        _features.spirv_shaders = true;
    }
}

void BackendOpenGL::_detectFeatures()
{
    // Baseline
    _features.major_version = 3;
    _features.minor_version = 3;
}

void BackendOpenGL::_createTextures(const RenderingPayload&)
{
    glGenTextures(1, &_glyphAtlas);
    glBindTexture(GL_TEXTURE_2D, _glyphAtlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint BackendOpenGL::_compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint BackendOpenGL::_linkProgram(GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        glDeleteProgram(program);
        program = 0;
    }

    glDetachShader(program, vs);
    glDetachShader(program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

void BackendOpenGL::_validateProgram(GLuint program)
{
    if (!program)
    {
        return;
    }
    glValidateProgram(program);
}
