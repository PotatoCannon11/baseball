#include "render/gl.h"

#include <SDL3/SDL.h>

#include <cstdio>

// clang-format off
PFNGLCLEARPROC glClear = nullptr;
PFNGLCLEARCOLORPROC glClearColor = nullptr;
PFNGLVIEWPORTPROC glViewport = nullptr;
PFNGLENABLEPROC glEnable = nullptr;
PFNGLDISABLEPROC glDisable = nullptr;
PFNGLDEPTHFUNCPROC glDepthFunc = nullptr;
PFNGLDEPTHMASKPROC glDepthMask = nullptr;
PFNGLBLENDFUNCPROC glBlendFunc = nullptr;
PFNGLCULLFACEPROC glCullFace = nullptr;
PFNGLFRONTFACEPROC glFrontFace = nullptr;
PFNGLDRAWARRAYSPROC glDrawArrays = nullptr;
PFNGLDRAWELEMENTSPROC glDrawElements = nullptr;
PFNGLGETERRORPROC glGetError = nullptr;
PFNGLGETINTEGERVPROC glGetIntegerv = nullptr;
PFNGLGETSTRINGPROC glGetString = nullptr;
PFNGLPOLYGONMODEPROC glPolygonMode = nullptr;
PFNGLPIXELSTOREIPROC glPixelStorei = nullptr;
PFNGLTEXPARAMETERIPROC glTexParameteri = nullptr;
PFNGLTEXPARAMETERFPROC glTexParameterf = nullptr;
PFNGLTEXIMAGE2DPROC glTexImage2D = nullptr;
PFNGLBINDTEXTUREPROC glBindTexture = nullptr;
PFNGLGENTEXTURESPROC glGenTextures = nullptr;
PFNGLDELETETEXTURESPROC glDeleteTextures = nullptr;
PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D = nullptr;
PFNGLSCISSORPROC glScissor = nullptr;

PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;

PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
PFNGLBUFFERDATAPROC glBufferData = nullptr;
PFNGLBUFFERSUBDATAPROC glBufferSubData = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;

PFNGLCREATESHADERPROC glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
PFNGLDELETESHADERPROC glDeleteShader = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
PFNGLATTACHSHADERPROC glAttachShader = nullptr;
PFNGLDETACHSHADERPROC glDetachShader = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLUNIFORM1IPROC glUniform1i = nullptr;
PFNGLUNIFORM1FPROC glUniform1f = nullptr;
PFNGLUNIFORM2FPROC glUniform2f = nullptr;
PFNGLUNIFORM3FPROC glUniform3f = nullptr;
PFNGLUNIFORM4FPROC glUniform4f = nullptr;
PFNGLUNIFORM3FVPROC glUniform3fv = nullptr;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;

PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = nullptr;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;
PFNGLDRAWBUFFERPROC glDrawBuffer = nullptr;
PFNGLREADBUFFERPROC glReadBuffer = nullptr;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = nullptr;
PFNGLGETSTRINGIPROC glGetStringi = nullptr;
// clang-format on

namespace render {

namespace {

template <typename T>
bool load_one(T* out, const char* name) {
    *out = reinterpret_cast<T>(reinterpret_cast<void*>(SDL_GL_GetProcAddress(name)));
    if (*out == nullptr) {
        std::fprintf(stderr, "render::gl_load: failed to load %s\n", name);
        return false;
    }
    return true;
}

}  // namespace

bool gl_load() {
    bool ok = true;
    // clang-format off
    ok &= load_one(&glClear, "glClear");
    ok &= load_one(&glClearColor, "glClearColor");
    ok &= load_one(&glViewport, "glViewport");
    ok &= load_one(&glEnable, "glEnable");
    ok &= load_one(&glDisable, "glDisable");
    ok &= load_one(&glDepthFunc, "glDepthFunc");
    ok &= load_one(&glDepthMask, "glDepthMask");
    ok &= load_one(&glBlendFunc, "glBlendFunc");
    ok &= load_one(&glCullFace, "glCullFace");
    ok &= load_one(&glFrontFace, "glFrontFace");
    ok &= load_one(&glDrawArrays, "glDrawArrays");
    ok &= load_one(&glDrawElements, "glDrawElements");
    ok &= load_one(&glGetError, "glGetError");
    ok &= load_one(&glGetIntegerv, "glGetIntegerv");
    ok &= load_one(&glGetString, "glGetString");
    ok &= load_one(&glPolygonMode, "glPolygonMode");
    ok &= load_one(&glPixelStorei, "glPixelStorei");
    ok &= load_one(&glTexParameteri, "glTexParameteri");
    ok &= load_one(&glTexParameterf, "glTexParameterf");
    ok &= load_one(&glTexImage2D, "glTexImage2D");
    ok &= load_one(&glBindTexture, "glBindTexture");
    ok &= load_one(&glGenTextures, "glGenTextures");
    ok &= load_one(&glDeleteTextures, "glDeleteTextures");
    ok &= load_one(&glTexSubImage2D, "glTexSubImage2D");
    ok &= load_one(&glScissor, "glScissor");

    ok &= load_one(&glActiveTexture, "glActiveTexture");

    ok &= load_one(&glGenBuffers, "glGenBuffers");
    ok &= load_one(&glBindBuffer, "glBindBuffer");
    ok &= load_one(&glBufferData, "glBufferData");
    ok &= load_one(&glBufferSubData, "glBufferSubData");
    ok &= load_one(&glDeleteBuffers, "glDeleteBuffers");

    ok &= load_one(&glCreateShader, "glCreateShader");
    ok &= load_one(&glShaderSource, "glShaderSource");
    ok &= load_one(&glCompileShader, "glCompileShader");
    ok &= load_one(&glGetShaderiv, "glGetShaderiv");
    ok &= load_one(&glGetShaderInfoLog, "glGetShaderInfoLog");
    ok &= load_one(&glDeleteShader, "glDeleteShader");
    ok &= load_one(&glCreateProgram, "glCreateProgram");
    ok &= load_one(&glAttachShader, "glAttachShader");
    ok &= load_one(&glDetachShader, "glDetachShader");
    ok &= load_one(&glLinkProgram, "glLinkProgram");
    ok &= load_one(&glGetProgramiv, "glGetProgramiv");
    ok &= load_one(&glGetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load_one(&glDeleteProgram, "glDeleteProgram");
    ok &= load_one(&glUseProgram, "glUseProgram");
    ok &= load_one(&glGetUniformLocation, "glGetUniformLocation");
    ok &= load_one(&glUniform1i, "glUniform1i");
    ok &= load_one(&glUniform1f, "glUniform1f");
    ok &= load_one(&glUniform2f, "glUniform2f");
    ok &= load_one(&glUniform3f, "glUniform3f");
    ok &= load_one(&glUniform4f, "glUniform4f");
    ok &= load_one(&glUniform3fv, "glUniform3fv");
    ok &= load_one(&glUniformMatrix4fv, "glUniformMatrix4fv");
    ok &= load_one(&glEnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= load_one(&glDisableVertexAttribArray, "glDisableVertexAttribArray");
    ok &= load_one(&glVertexAttribPointer, "glVertexAttribPointer");

    ok &= load_one(&glGenVertexArrays, "glGenVertexArrays");
    ok &= load_one(&glBindVertexArray, "glBindVertexArray");
    ok &= load_one(&glDeleteVertexArrays, "glDeleteVertexArrays");
    ok &= load_one(&glGenFramebuffers, "glGenFramebuffers");
    ok &= load_one(&glBindFramebuffer, "glBindFramebuffer");
    ok &= load_one(&glFramebufferTexture2D, "glFramebufferTexture2D");
    ok &= load_one(&glCheckFramebufferStatus, "glCheckFramebufferStatus");
    ok &= load_one(&glDeleteFramebuffers, "glDeleteFramebuffers");
    ok &= load_one(&glDrawBuffer, "glDrawBuffer");
    ok &= load_one(&glReadBuffer, "glReadBuffer");
    ok &= load_one(&glGenerateMipmap, "glGenerateMipmap");
    ok &= load_one(&glGetStringi, "glGetStringi");
    // clang-format on
    return ok;
}

}  // namespace render
