#include "GLCore.hpp"
#include <iostream>

PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
PFNGLACTIVETEXTUREPROC rtsGlActiveTexture = nullptr;
PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
PFNGLBUFFERDATAPROC glBufferData = nullptr;
PFNGLBUFFERSUBDATAPROC glBufferSubData = nullptr;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
PFNGLCREATESHADERPROC glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
PFNGLATTACHSHADERPROC glAttachShader = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
PFNGLDELETESHADERPROC glDeleteShader = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
PFNGLUNIFORM4FPROC glUniform4f = nullptr;
PFNGLUNIFORM3FPROC glUniform3f = nullptr;
PFNGLUNIFORM1FPROC glUniform1f = nullptr;
PFNGLUNIFORM1IPROC glUniform1i = nullptr;

RTSGLGenFramebuffersProc rtsGlGenFramebuffers=nullptr;
RTSGLBindFramebufferProc rtsGlBindFramebuffer=nullptr;
RTSGLDeleteFramebuffersProc rtsGlDeleteFramebuffers=nullptr;
RTSGLFramebufferTexture2DProc rtsGlFramebufferTexture2D=nullptr;
RTSGLCheckFramebufferStatusProc rtsGlCheckFramebufferStatus=nullptr;
RTSGLGenRenderbuffersProc rtsGlGenRenderbuffers=nullptr;
RTSGLBindRenderbufferProc rtsGlBindRenderbuffer=nullptr;
RTSGLRenderbufferStorageProc rtsGlRenderbufferStorage=nullptr;
RTSGLFramebufferRenderbufferProc rtsGlFramebufferRenderbuffer=nullptr;
RTSGLDeleteRenderbuffersProc rtsGlDeleteRenderbuffers=nullptr;

bool loadOpenGLFunctions() {
    rtsGlGenFramebuffers=reinterpret_cast<RTSGLGenFramebuffersProc>(glfwGetProcAddress("glGenFramebuffers"));
    rtsGlBindFramebuffer=reinterpret_cast<RTSGLBindFramebufferProc>(glfwGetProcAddress("glBindFramebuffer"));
    rtsGlDeleteFramebuffers=reinterpret_cast<RTSGLDeleteFramebuffersProc>(glfwGetProcAddress("glDeleteFramebuffers"));
    rtsGlFramebufferTexture2D=reinterpret_cast<RTSGLFramebufferTexture2DProc>(glfwGetProcAddress("glFramebufferTexture2D"));
    rtsGlCheckFramebufferStatus=reinterpret_cast<RTSGLCheckFramebufferStatusProc>(glfwGetProcAddress("glCheckFramebufferStatus"));
    rtsGlGenRenderbuffers=reinterpret_cast<RTSGLGenRenderbuffersProc>(glfwGetProcAddress("glGenRenderbuffers"));
    rtsGlBindRenderbuffer=reinterpret_cast<RTSGLBindRenderbufferProc>(glfwGetProcAddress("glBindRenderbuffer"));
    rtsGlRenderbufferStorage=reinterpret_cast<RTSGLRenderbufferStorageProc>(glfwGetProcAddress("glRenderbufferStorage"));
    rtsGlFramebufferRenderbuffer=reinterpret_cast<RTSGLFramebufferRenderbufferProc>(glfwGetProcAddress("glFramebufferRenderbuffer"));
    rtsGlDeleteRenderbuffers=reinterpret_cast<RTSGLDeleteRenderbuffersProc>(glfwGetProcAddress("glDeleteRenderbuffers"));
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)glfwGetProcAddress("glDeleteBuffers");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)glfwGetProcAddress("glDeleteVertexArrays");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)glfwGetProcAddress("glDeleteProgram");
    rtsGlActiveTexture = (PFNGLACTIVETEXTUREPROC)glfwGetProcAddress("glActiveTexture");
    glGenBuffers = (PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)glfwGetProcAddress("glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)glfwGetProcAddress("glBufferSubData");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glfwGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)glfwGetProcAddress("glBindVertexArray");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glfwGetProcAddress("glEnableVertexAttribArray");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)glfwGetProcAddress("glVertexAttribPointer");
    glCreateShader = (PFNGLCREATESHADERPROC)glfwGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)glfwGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)glfwGetProcAddress("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)glfwGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)glfwGetProcAddress("glGetShaderInfoLog");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)glfwGetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)glfwGetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)glfwGetProcAddress("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)glfwGetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)glfwGetProcAddress("glGetProgramInfoLog");
    glUseProgram = (PFNGLUSEPROGRAMPROC)glfwGetProcAddress("glUseProgram");
    glDeleteShader = (PFNGLDELETESHADERPROC)glfwGetProcAddress("glDeleteShader");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)glfwGetProcAddress("glGetUniformLocation");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)glfwGetProcAddress("glUniformMatrix4fv");
    glUniform4f = (PFNGLUNIFORM4FPROC)glfwGetProcAddress("glUniform4f");
    glUniform3f = (PFNGLUNIFORM3FPROC)glfwGetProcAddress("glUniform3f");
    glUniform1f = (PFNGLUNIFORM1FPROC)glfwGetProcAddress("glUniform1f");
    glUniform1i = (PFNGLUNIFORM1IPROC)glfwGetProcAddress("glUniform1i");

    bool ok = true;
#define REQUIRE_GL(fn) if (!(fn)) { std::cerr << "Missing OpenGL function: " #fn << '\n'; ok = false; }
    REQUIRE_GL(rtsGlGenFramebuffers)
    REQUIRE_GL(rtsGlBindFramebuffer)
    REQUIRE_GL(rtsGlDeleteFramebuffers)
    REQUIRE_GL(rtsGlFramebufferTexture2D)
    REQUIRE_GL(rtsGlCheckFramebufferStatus)
    REQUIRE_GL(rtsGlGenRenderbuffers)
    REQUIRE_GL(rtsGlBindRenderbuffer)
    REQUIRE_GL(rtsGlRenderbufferStorage)
    REQUIRE_GL(rtsGlFramebufferRenderbuffer)
    REQUIRE_GL(rtsGlDeleteRenderbuffers)
    REQUIRE_GL(glDeleteBuffers)
    REQUIRE_GL(glDeleteVertexArrays)
    REQUIRE_GL(glDeleteProgram)
    REQUIRE_GL(rtsGlActiveTexture)
    REQUIRE_GL(glGenBuffers)
    REQUIRE_GL(glBindBuffer)
    REQUIRE_GL(glBufferData)
    REQUIRE_GL(glBufferSubData)
    REQUIRE_GL(glGenVertexArrays)
    REQUIRE_GL(glBindVertexArray)
    REQUIRE_GL(glEnableVertexAttribArray)
    REQUIRE_GL(glVertexAttribPointer)
    REQUIRE_GL(glCreateShader)
    REQUIRE_GL(glShaderSource)
    REQUIRE_GL(glCompileShader)
    REQUIRE_GL(glGetShaderiv)
    REQUIRE_GL(glGetShaderInfoLog)
    REQUIRE_GL(glCreateProgram)
    REQUIRE_GL(glAttachShader)
    REQUIRE_GL(glLinkProgram)
    REQUIRE_GL(glGetProgramiv)
    REQUIRE_GL(glGetProgramInfoLog)
    REQUIRE_GL(glUseProgram)
    REQUIRE_GL(glDeleteShader)
    REQUIRE_GL(glGetUniformLocation)
    REQUIRE_GL(glUniformMatrix4fv)
    REQUIRE_GL(glUniform4f)
    REQUIRE_GL(glUniform3f)
    REQUIRE_GL(glUniform1f)
    REQUIRE_GL(glUniform1i)
#undef REQUIRE_GL
    return ok;
}