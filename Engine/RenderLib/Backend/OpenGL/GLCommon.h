#ifndef __GL_COMMON_H__
#define __GL_COMMON_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>

#if defined( _WIN32 )
#if _MSC_VER
#pragma warning (disable: 4201)
#pragma warning (disable: 4214)
#pragma warning (disable: 4514)
#pragma warning (disable: 4032)
#pragma warning (disable: 4201)
#pragma warning (disable: 4214)
#endif
#include <windows.h>
#include <GL/gl.h>
#elif defined( __linux__ ) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined( __sun )
#include <GL/gl.h>
#include <GL/glcorearb.h>
#include <GL/glext.h>
#include <KHR/khrplatform.h>
#elif defined(__APPLE__)
#define GL_NUM_EXTENSIONS                 0x821D
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#endif

#pragma once

typedef void (APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);
typedef void (APIENTRY *GLDEBUGPROCAMD)(GLuint id, GLenum category, GLenum severity, GLsizei length, const GLchar *message, GLvoid *userParam);
typedef void (APIENTRY *GLDEBUGPROCKHR)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const GLvoid *userParam);
typedef void (APIENTRY *GLDEBUGPROCARB)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const GLvoid *userParam);

typedef void*(*NGLloadproc)(const char *name);

// GL function loader, based on https://gist.github.com/rygorous/16796a0c876cf8a5f542caddb55bce8a
#define NGL_Core_Procs \
    NGL( void, glColorMask, GLenum red, GLenum green, GLenum blue, GLenum alpha ) \
    NGL( void, glAlphaFunc, GLenum func, GLclampf ref ) \
    NGL( GLenum, glGetError, void ) \
    NGL( void, glGetIntegerv, GLenum pname, GLint *data ) \
    NGL( void, glGetFloatv, GLenum pname, GLfloat *data ) \
    NGL( void, glEnable, GLenum mode ) \
    NGL( void, glDisable, GLenum mode ) \
    NGL( void, glBlendFunc, GLenum sfactor, GLenum dfactor ) \
    NGL( const GLubyte*, glGetString, GLenum name ) \
    NGL( const GLubyte*, glGetStringi, GLenum name, GLuint index ) \
    NGL( void, glDepthMask, GLboolean flag ) \
    NGL( void, glDepthFunc, GLenum func ) \
    NGL( void, glReadPixels, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels ) \
	NGL( void, glScissor, GLint x, GLint y, GLsizei width, GLsizei height ) \
	NGL( void, glShadeModel, GLenum mode ) \
	NGL( void, glStencilFunc, GLenum func, GLint ref, GLuint mask ) \
	NGL( void, glStencilMask, GLuint mask ) \
	NGL( void, glStencilOp, GLenum fail, GLenum zfail, GLenum zpass ) \
    NGL( void, glClearStencil, GLint s ) \
    NGL( void, glViewport, GLint x, GLint y, GLsizei width, GLsizei height ) \
    NGL( void, glClear, GLbitfield mask ) \
    NGL( void, glDrawArrays, GLenum mode, GLint first, GLsizei count ) \
    NGL( void, glDrawElements, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices ) \
    NGL( void, glLineWidth, GLfloat width ) \
    NGL( void, glCullFace, GLenum mode ) \
    NGL( void, glGetBooleanv, GLenum pname, GLboolean *params ) \
    NGL( void, glBlendEquation, GLenum mode ) \
    NGL( void, glBlendFuncSeparate, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha ) \
    NGL( void, glBlendEquationSeparate, GLenum modeRGB, GLenum modeAlpha ) \
    NGL( void, glPolygonMode, GLenum face, GLenum mode ) \
    NGL( void, glPixelStorei, GLenum pname, GLint param ) \
    NGL( GLboolean, glIsEnabled, GLenum cap ) \
    NGL( GLboolean, glIsDisabled, GLenum cap ) \
    NGL( GLboolean, glIsProgram, GLenum program ) \
    NGL( GLboolean, glIsShader, GLenum shader ) \
    NGL( void, glDrawBuffer, GLenum mode ) \
    NGL( void, glDepthRange, GLclampf zNear, GLclampf zFar ) \
    NGL( void, glDrawBuffers, GLsizei n, const GLenum *bufs ) \
    NGL( void, glBegin, GLenum mode ) \
    NGL( void, glEnd, void ) \
    NGL( void, glFlush, void ) \
    NGL( void, glFinish, void ) \
    NGL( void, glClearColor, GLfloat r, GLfloat g, GLfloat b, GLfloat a ) \
    NGL( void, glColor4f, GLfloat r, GLfloat g, GLfloat b, GLfloat a ) \
    NGL( void, glColor3f, GLfloat r, GLfloat g, GLfloat b ) \
    NGL( void, glColor4fv, const GLfloat *values ) \
    NGL( void, glColor3fv, const GLfloat *values ) \
    NGL( void, glColor4us, GLushort r, GLushort g, GLushort blue, GLushort alpha ) \
    NGL( void, glVertex3fv, const GLfloat *values ) \
    NGL( void, glVertex3f, GLfloat x, GLfloat y, GLfloat z ) \
    NGL( void, glVertex2f, GLfloat x, GLfloat y ) \
    NGL( void, glVertex2fv, const GLfloat *values ) \
    NGL( void, glTexCoord2f, GLfloat u, GLfloat v ) \
    NGL( void, glTexCoord2fv, const GLfloat *values ) \
    NGL( void, glPushMatrix, void ) \
    NGL( void, glPopMatrix, void ) \
    NGL( void, glMatrixMode, GLenum mode ) \
    NGL( void, glMultMatrixf, const GLfloat *m ) \
    NGL( void, glMultMatrixd, const GLdouble *m ) \
    NGL( void, glLoadIdentity, void ) \
    NGL( void, glLoadMatrixf, const GLfloat *m ) \
    NGL( void, glLoadMatrixd, const GLdouble *m ) \
    NGL( void, glEnableClientState, GLenum cap ) \
    NGL( void, glDisableClientState, GLenum cap ) \
    NGL( void, glTexCoordPointer, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer ) \
    NGL( void, glVertexPointer, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer ) \
    NGL( void, glColorPointer, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer ) \
    NGL( void, glIndexPointer, GLenum type, GLsizei stride, const GLvoid *pointer ) \
    NGL( void, glScalef, GLfloat x, GLfloat y, GLfloat z ) \
    NGL( void, glRotatef, GLfloat angle, GLfloat x, GLfloat y, GLfloat z ) \
    NGL( void, glTranslatef, GLfloat x, GLfloat y, GLfloat z ) \
    NGL( void, glDrawElementsBaseVertex, GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex ) \
    NGL( void, glGenQueries, GLsizei n, GLuint *ids ) \
    NGL( void, glDeleteQueries, GLsizei n, const GLuint *ids ) \
    NGL( void, glBeginQuery, GLenum target, GLuint id ) \
    NGL( void, glEndQuery, GLenum target ) \
    NGL( void, glGetQueryObjectiv, GLuint id, GLenum pname, GLuint *params ) \
    NGL( void, glOrtho, GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat far, GLfloat near ) \

#define NGL_List_Procs \
    NGL( void, glListBase, GLuint base ) \
    NGL( void, glNewList, GLuint list, GLenum mode ) \
    NGL( void, glCallList, GLuint list ) \
    NGL( void, glCallLists, GLsizei n, GLenum type, const GLvoid *lists ) \
    NGL( void, glGenLists, GLsizei range ) \
    NGL( void, glDeleteLists, GLuint list, GLsizei range ) \
    NGL( void, glEndList, void )

#define NGL_Debug_Procs \
    NGL( void, glDebugMessageControlARB, GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled ) \
    NGL( void, glDebugMessageCallbackARB, GLDEBUGPROCARB callback, const GLvoid *userParam ) \
    NGL( void, glDebugMessageInsertARB, GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf ) \
    NGL( void, glDebugMessageEnableAMD, GLenum category, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled ) \
    NGL( void, glDebugMessageCallbackAMD, GLDEBUGPROCAMD callback, void *userParam ) \
    NGL( void, glDebugMessageControlKHR, GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled ) \
    NGL( void, glDebugMessageCallbackKHR, GLDEBUGPROCKHR callback, const void *userParam ) \
    NGL( void, glDebugMessageInsertKHR, GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf ) \
    NGL( void, glDebugMessageControl, GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled ) \
    NGL( void, glDebugMessageCallback, GLDEBUGPROC callback, const void *userParam ) \
    NGL( void, glDebugMessageInsert, GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf ) \
    NGL( void, glObjectLabel, GLenum identifier, GLuint name, GLsizei length, const GLchar *label ) \
    NGL( void, glPushDebugGroup, GLenum source, GLuint id, GLsizei length, const GLchar *label ) \
    NGL( void, glPopDebugGroup, void ) \

#define NGL_GLSL_SPIRV_Procs \
    NGL( void, glShaderBinary, GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length ) \
    NGL( void, glGetProgramBinary, GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary )

#define NGL_Shader_Procs \
    NGL( void, glBindAttribLocation, GLhandleARB programObj, GLuint index, const GLcharARB *name ) \
    NGL( GLint, glGetAttribLocation, GLuint program, const GLchar *name ) \
    NGL( void, glShaderSource, GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length ) \
    NGL( GLuint, glCreateProgram, void ) \
    NGL( void, glLinkProgram, GLuint program ) \
    NGL( void, glUseProgram, GLuint program ) \
    NGL( void, glDetachShader, GLuint program, GLuint shader ) \
    NGL( GLuint, glCreateShader, GLenum type ) \
    NGL( void, glDeleteShader, GLuint shader ) \
    NGL( void, glDeleteProgram, GLuint program ) \
    NGL( void, glGetShaderiv, GLuint shader, GLenum pname, GLint *params ) \
    NGL( void, glGetProgramiv, GLuint program, GLenum pname, GLint *params ) \
    NGL( void, glGetShaderInfoLog, GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog ) \
    NGL( GLint, glGetUniformLocation, GLuint program, const GLchar *name ) \
    NGL( void, glUniform1f, GLint location, GLfloat v0 ) \
    NGL( void, glUniform2f, GLint location, GLfloat v0, GLfloat v1 ) \
    NGL( void, glUniform3f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2 ) \
    NGL( void, glUniform4f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3 ) \
    NGL( void, glUniform1fv, GLint location, GLsizei count, const GLfloat *value ) \
    NGL( void, glUniform2fv, GLint location, GLsizei count, const GLfloat *value ) \
    NGL( void, glUniform3fv, GLint location, GLsizei count, const GLfloat *value ) \
    NGL( void, glUniform4fv, GLint location, GLsizei count, const GLfloat *value ) \
    NGL( void, glUniform1i, GLint location, GLint v0 ) \
    NGL( void, glUniform2i, GLint location, GLint v0, GLint v1 ) \
    NGL( void, glUniform3i, GLint location, GLint v0, GLint v1, GLint v2 ) \
    NGL( void, glUniform4i, GLint location, GLint v0, GLint v1, GLint v2, GLint v3 ) \
    NGL( void, glUniform1iv, GLint location, GLsizei count, const GLint *value ) \
    NGL( void, glUniform2iv, GLint location, GLsizei count, const GLint *value ) \
    NGL( void, glUniform3iv, GLint location, GLsizei count, const GLint *value ) \
    NGL( void, glUniform4iv, GLint location, GLsizei count, const GLint *value ) \
    NGL( void, glUniform1ui, GLint location, GLuint v0 ) \
    NGL( void, glUniform2ui, GLint location, GLuint v0, GLuint v1 ) \
    NGL( void, glUniform3ui, GLint location, GLuint v0, GLuint v1, GLuint v2 ) \
    NGL( void, glUniform4ui, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3 ) \
    NGL( void, glUniform1uiv, GLint location, GLsizei count, const GLuint *value ) \
    NGL( void, glUniform2uiv, GLint location, GLsizei count, const GLuint *value ) \
    NGL( void, glUniform3uiv, GLint location, GLsizei count, const GLuint *value ) \
    NGL( void, glUniform4uiv, GLint location, GLsizei count, const GLuint *value ) \
    NGL( void, glUniformMatrix3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value ) \
    NGL( void, glUniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value ) \
    NGL( void, glGetProgramInfoLog, GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog ) \
    NGL( void, glCompileShader, GLuint shader ) \
    NGL( void, glAttachShader, GLuint program, GLuint shader ) \
    NGL( void, glValidateProgram, GLuint program ) \
    NGL( GLuint, glGetUniformBlockIndex, GLuint program, const GLchar *uniformBlockName ) \
    NGL( void, glUniformBlockBinding, GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding ) \
    NGL( void, glGetShaderSource, GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source ) \
    NGL( void, glGetActiveUniform, GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name ) \
    NGL( void, glMinSampleShadingARB, GLclampf value )

#define NGL_Texture_Procs \
    NGL( void, glTexImage2DMultisample, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations ) \
    NGL( void, glActiveTexture, GLenum texture ) \
    NGL( void, glGenTextures, GLsizei n, GLuint *textures ) \
    NGL( void, glDeleteTextures, GLsizei n, const GLuint *textures ) \
    NGL( void, glBindTexture, GLenum target, GLuint texture ) \
    NGL( void, glTexImage2D, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels ) \
    NGL( void, glTexParameteri, GLenum target, GLenum pname, GLint param ) \
    NGL( void, glTexParameterf, GLenum target, GLenum pname, GLfloat param ) \
    NGL( void, glCompressedTexImage2D,GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data ) \
    NGL( void, glCompressedTexSubImage2D, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data ) \
    NGL( void, glTexSubImage2D, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * data ) \
    NGL( void, glGenSamplers, GLsizei n, GLuint *samplers ) \
    NGL( void, glBindSampler, GLuint unit, GLuint sampler ) \
    NGL( void, glDeleteSamplers, GLsizei n, const GLuint *samplers ) \
    NGL( void, glSamplerParameteri, GLuint sampler, GLenum pname, GLint param ) \
    NGL( void, glSamplerParameterf, GLuint sampler, GLenum pname, GLfloat param ) \
    NGL( void, glGenerateMipmap, GLenum target ) \
    NGL( void, glBufferPageCommitmentARB, GLenum target, GLintptr offset, GLsizeiptr size, GLboolean commit )

#define NGL_FBO_Procs \
    NGL( void, glGenFramebuffers, GLsizei n, GLuint *buffers ) \
    NGL( void, glDeleteFramebuffers, GLsizei n, const GLuint *buffers ) \
    NGL( void, glGenRenderbuffers, GLsizei n, GLuint *buffers ) \
    NGL( void, glDeleteRenderbuffers, GLsizei n, const GLuint *buffers ) \
    NGL( void, glBindFramebuffer, GLenum target, GLuint buffer ) \
    NGL( void, glBindRenderbuffer, GLenum target, GLuint buffer ) \
    NGL( void, glRenderbufferStorage, GLenum target, GLenum internalformat, GLsizei width, GLsizei height ) \
    NGL( void, glRenderbufferStorageMultisample, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height ) \
    NGL( void, glBlitFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter ) \
    NGL( void, glFramebufferRenderbuffer, GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer ) \
    NGL( void, glFramebufferTexture2D, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level ) \
    NGL( GLenum, glCheckFramebufferStatus, GLenum target ) \
    NGL( void, glRenderBufferStorageMultisampleCoverageNV, GLenum target, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height ) \
    NGL( void, glGetFramebufferAttachmentParameteriv, GLenum target, GLenum attachment, GLenum pname, GLint *params ) \

#define NGL_VertexArrayARB_Procs \
    NGL( void, glEnableVertexArrayAttribARB, GLuint vao, GLuint index ) \
    NGL( void, glDisableVertexArrayAttribARB, GLuint vao, GLuint index ) \
    NGL( void, glEnableVertexAttribArrayARB, GLuint index ) \
    NGL( void, glDisableVertexAttribArrayARB, GLuint index ) \
    NGL( void, glVertexAttribPointerARB, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer ) \
    NGL( void, glGetVertexAttribivARB, GLuint index, GLenum pname, GLint *params ) \
    NGL( void, glGetVertexAttribPointervARB, GLuint index, GLenum pname, void **pointer )

#define NGL_VertexArray_Procs \
    NGL( void, glGenVertexArrays, GLsizei n, GLuint *arrays ) \
    NGL( void, glDeleteVertexArrays, GLsizei n, const GLuint *arrays ) \
    NGL( void, glBindVertexArray, GLuint array ) \
    NGL( void, glEnableVertexAttribArray, GLuint index ) \
    NGL( void, glEnableVertexArrayAttrib, GLuint vao, GLuint index ) \
    NGL( void, glDisableVertexArrayAttrib, GLuint vao, GLuint index ) \
    NGL( void, glDisableVertexAttribArray, GLuint index ) \
    NGL( void, glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer ) \
    NGL( void, glGetVertexAttribiv, GLuint index, GLenum pname, GLint *params ) \
    NGL( void, glGetVertexAttribPointerv, GLuint index, GLenum pname, void **pointer )

#define NGL_BufferARB_Procs \
    NGL( void, glGenBuffersARB, GLsizei n, GLuint *buffers ) \
    NGL( void, glDeleteBuffersARB, GLsizei n, const GLuint *buffers ) \
    NGL( void, glBindBufferARB, GLenum target, GLuint buffer ) \
    NGL( void, glBufferDataARB, GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage ) \
    NGL( void, glBufferSubDataARB, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data ) \
    NGL( void, glUnmapBufferARB, GLenum target ) \
    NGL( void*, glMapBufferARB, GLenum target, GLbitfield access ) \
    NGL( void, glBindBufferRangeARB, GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size ) \
    NGL( void, glBindBufferBaseARB, GLenum target, GLuint index, GLuint buffer )

#define NGL_Buffer_Procs \
    NGL( void, glGenBuffers, GLsizei n, GLuint *buffers ) \
    NGL( void, glDeleteBuffers, GLsizei n, const GLuint *buffers ) \
    NGL( void, glBindBuffer, GLenum target, GLuint buffer ) \
    NGL( void, glBufferData, GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage ) \
    NGL( void, glBufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data ) \
    NGL( void*, glMapBuffer, GLenum target, GLbitfield access ) \
    NGL( void, glUnmapBuffer, GLenum target ) \
    NGL( void, glBindBufferRange, GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size ) \
    NGL( void, glBindBufferBase, GLenum target, GLuint index, GLuint buffer ) \
    NGL( void, glCreateBuffers, GLsizei n, GLuint *buffers ) \
    NGL( void, glGetBufferParameteriv, GLenum target, GLenum value, GLint *data ) \
    NGL( void, glCopyBufferSubData, GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, \
        GLsizeiptr size  )

#define NGL_VertexShaderARB_Procs \
    NGL( void, glBindAttribLocationARB, GLhandleARB programObj, GLuint index, const GLcharARB *name) \
    NGL( void, glGetActiveAttribARB, GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name) \
    NGL( GLint, glGetAttribLocationARB, GLhandleARB programObj, const GLcharARB *name)

/*
* GL_EXT, GL_ARB procs
*/

#define NGL_ARB_sync \
    NGL( GLsync, glFenceSync, GLenum condition, GLbitfield flags ) \
    NGL( GLenum, glClientWaitSync, GLsync sync, GLbitfield flags, GLuint64 timeout ) \
    NGL( void, glWaitSync, GLsync sync, GLbitfield flags, GLuint64 timeout ) \
    NGL( void, glDeleteSync, GLsync sync )

#define NGL_ARB_map_buffer_range \
    NGL( void*, glMapBufferRange, GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access ) \
    NGL( void, glFlushMappedBufferRange, GLenum target, GLintptr offset, GLsizeiptr length ) \
    NGL( void, glInvalidateBufferData, GLuint buffer ) \

#define NGL_ARB_buffer_storage \
    NGL( void, glBufferStorage, GLenum target, GLsizeiptr size, const void *data, GLbitfield flags )

#define NGL_Procs \
    NGL_Core_Procs \
    NGL_Debug_Procs \
    NGL_Shader_Procs \
    NGL_FBO_Procs \
    NGL_VertexArray_Procs \
    NGL_Buffer_Procs \
    NGL_Texture_Procs \
    NGL_VertexArrayARB_Procs \
    NGL_VertexShaderARB_Procs \
    NGL_BufferARB_Procs \

#ifdef SIRENGINE_DEBUG
#define NGL( ret, name, ... )\
    typedef ret (APIENTRYP PFN ## name) (__VA_ARGS__); \
    extern PFN ## name n ## name;
#else
#define NGL( ret, name, ... )\
    typedef ret (APIENTRYP PFN ## name) (__VA_ARGS__); \
    extern PFN ## name n ## name;
#endif
NGL_Core_Procs
NGL_Debug_Procs
NGL_Shader_Procs
NGL_GLSL_SPIRV_Procs
NGL_FBO_Procs
NGL_VertexArray_Procs
NGL_VertexArrayARB_Procs
NGL_VertexShaderARB_Procs
NGL_BufferARB_Procs
NGL_Buffer_Procs
NGL_Texture_Procs

NGL_ARB_map_buffer_range
NGL_ARB_buffer_storage
NGL_ARB_sync
#undef NGL

#define LOAD_GL_PROCS( procs ) procs

class GLProgram;
class GLShaderBuffer;

typedef struct GLPipelineSet {
    GLProgram *pShader;
    uint32_t nEnabledAttribs;

    CVector<GLShaderBuffer *> dataInputBuffers;
} GLPipelineSet_t;

#endif