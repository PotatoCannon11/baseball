#pragma once

// Minimal hand-written OpenGL 4.1 core function loader.
//
// The project spec names glad as the GL loader, but generating glad's
// output needs Python's pip/venv, which isn't available in this dev
// environment without a sudo install. Rather than block on that, this
// hand-written loader declares only the ~50 GL functions the renderer
// actually calls (a full glad-generated loader would include every GL 4.1
// function whether used or not) and loads them the exact same way glad
// does: via SDL_GL_GetProcAddress into function-pointer variables that
// shadow the real function names, so calling code (glClear(...), etc.)
// looks and works identically either way.
//
// third_party/khronos/glcorearb.h is the official Khronos registry header
// (MIT-licensed, unmodified) -- vendored purely for its type definitions,
// GL constants, and PFNGL*PROC function-pointer typedefs, which are the
// authoritative source of truth for exact GL signatures. It is included
// WITHOUT defining GL_GLEXT_PROTOTYPES, which keeps it from declaring its
// own `extern` function symbols, so there's no conflict with the function
// pointer variables declared below.
#include "glcorearb.h"

namespace render {

// Loads every function pointer below via SDL_GL_GetProcAddress. Must be
// called once, after an OpenGL context is current. Returns false (and
// leaves an error on stderr) if any required function failed to load --
// treat that as fatal, don't try to render with a partially loaded API.
bool gl_load();

}  // namespace render

// clang-format off

// --- 1.0 / 1.1 : core state, drawing, textures ------------------------
extern PFNGLCLEARPROC glClear;
extern PFNGLCLEARCOLORPROC glClearColor;
extern PFNGLVIEWPORTPROC glViewport;
extern PFNGLENABLEPROC glEnable;
extern PFNGLDISABLEPROC glDisable;
extern PFNGLDEPTHFUNCPROC glDepthFunc;
extern PFNGLDEPTHMASKPROC glDepthMask;
extern PFNGLBLENDFUNCPROC glBlendFunc;
extern PFNGLCULLFACEPROC glCullFace;
extern PFNGLFRONTFACEPROC glFrontFace;
extern PFNGLDRAWARRAYSPROC glDrawArrays;
extern PFNGLDRAWELEMENTSPROC glDrawElements;
extern PFNGLGETERRORPROC glGetError;
extern PFNGLGETINTEGERVPROC glGetIntegerv;
extern PFNGLGETSTRINGPROC glGetString;
extern PFNGLPOLYGONMODEPROC glPolygonMode;
extern PFNGLPIXELSTOREIPROC glPixelStorei;
extern PFNGLTEXPARAMETERIPROC glTexParameteri;
extern PFNGLTEXPARAMETERFPROC glTexParameterf;
extern PFNGLTEXIMAGE2DPROC glTexImage2D;
extern PFNGLBINDTEXTUREPROC glBindTexture;
extern PFNGLGENTEXTURESPROC glGenTextures;
extern PFNGLDELETETEXTURESPROC glDeleteTextures;
extern PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
extern PFNGLSCISSORPROC glScissor;

// --- 1.3 : multitexture -------------------------------------------------
extern PFNGLACTIVETEXTUREPROC glActiveTexture;

// --- 1.5 : buffer objects -----------------------------------------------
extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLBUFFERSUBDATAPROC glBufferSubData;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;

// --- 2.0 : shaders, programs, vertex attribs ----------------------------
extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLDETACHSHADERPROC glDetachShader;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLUNIFORM3FPROC glUniform3f;
extern PFNGLUNIFORM4FPROC glUniform4f;
extern PFNGLUNIFORM3FVPROC glUniform3fv;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;

// --- 3.0 : VAOs, framebuffers, mipmaps, indexed string queries ---------
extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
extern PFNGLDRAWBUFFERPROC glDrawBuffer;
extern PFNGLREADBUFFERPROC glReadBuffer;
extern PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
extern PFNGLGETSTRINGIPROC glGetStringi;

// clang-format on
