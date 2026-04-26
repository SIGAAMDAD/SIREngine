#ifndef __GL_PROCS_H__
#define __GL_PROCS_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Pch.h"

typedef void (APIENTRY *GLDEBUGPROC)( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam );
typedef void (APIENTRY *GLDEBUGPROCAMD)( GLuint id, GLenum category, GLenum severity, GLsizei length, const GLchar *message, GLvoid *userParam );
typedef void (APIENTRY *GLDEBUGPROCKHR)( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const GLvoid *userParam );
typedef void (APIENTRY *GLDEBUGPROCARB)( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const GLvoid *userParam );

typedef void*(*NGLloadproc)( const char *name );

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
	NGL( void, glReadPixels, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels ) \
	NGL( void, glScissor, GLint x, GLint y, GLsizei width, GLsizei height ) \
	NGL( void, glShadeModel, GLenum mode ) \
	NGL( void, glStencilFunc, GLenum func, GLint ref, GLuint mask ) \
	NGL( void, glStencilMask, GLuint mask ) \
	NGL( void, glStencilOp, GLenum fail, GLenum zfail, GLenum zpass ) \
	NGL( void, glClearStencil, GLint s ) \
	NGL( void, glViewport, GLint x, GLint y, GLsizei width, GLsizei height ) \
	NGL( void, glClear, GLbitfield mask ) \
	NGL( void, glDrawArrays, GLenum mode, GLint first, GLsizei count ) \
	NGL( void, glDrawElements, GLenum mode, GLsizei count, GLenum type, const void *indices ) \
	NGL( void, glLineWidth, GLfloat width ) \
	NGL( void, glCullFace, GLenum mode ) \
	NGL( void, glGetBooleanv, GLenum pname, GLboolean *params ) \
	NGL( void, glBlendEquation, GLenum mode ) \
	NGL( void, glBlendFuncSeparate, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha ) \
	NGL( void, glBlendEquationSeparate, GLenum modeRGB, GLenum modeAlpha ) \
	NGL( void, glPolygonMode, GLenum face, GLenum mode ) \
	NGL( void, glPixelStorei, GLenum pname, GLint param ) \
	NGL( GLboolean, glIsEnabled, GLenum cap ) \
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
	NGL( void, glTexCoordPointer, GLint size, GLenum type, GLsizei stride, const void *pointer ) \
	NGL( void, glVertexPointer, GLint size, GLenum type, GLsizei stride, const void *pointer ) \
	NGL( void, glColorPointer, GLint size, GLenum type, GLsizei stride, const void *pointer ) \
	NGL( void, glIndexPointer, GLenum type, GLsizei stride, const void *pointer ) \
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
	NGL( void, glDispatchCompute, GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z ) \
	NGL( void, glMemoryBarrier, GLbitfield barriers ) \
	NGL( void, glDrawElementsInstanced, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount ) \
	NGL( void, glDrawArraysInstanced, GLenum mode, GLint first, GLsizei count, GLsizei instancecount ) \
	NGL( void, glGetIntegeri_v, GLenum target, GLuint index, GLint *data ) \

#define NGL_List_Procs \
	NGL( void, glListBase, GLuint base ) \
	NGL( void, glNewList, GLuint list, GLenum mode ) \
	NGL( void, glCallList, GLuint list ) \
	NGL( void, glCallLists, GLsizei n, GLenum type, const void *lists ) \
	NGL( void, glGenLists, GLsizei range ) \
	NGL( void, glDeleteLists, GLuint list, GLsizei range ) \
	NGL( void, glEndList, void )

#define NGL_Debug_Procs \
	NGL( void, glDebugMessageControlARB, GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled ) \
	NGL( void, glDebugMessageCallbackARB, GLDEBUGPROCARB callback, const void *userParam ) \
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
	NGL( void, glGetProgramBinary, GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary ) \
	NGL( void, glProgramBinary, GLuint program, GLenum binaryFormat, const void *binary, GLsizei length ) \

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
	NGL( void, glMinSampleShadingARB, GLclampf value ) \
	NGL( void, glShaderStorageBlockBinding, GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding ) \

#define NGL_Texture_Procs \
	NGL( void, glTexImage2DMultisample, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations ) \
	NGL( void, glActiveTexture, GLenum texture ) \
	NGL( void, glGenTextures, GLsizei n, GLuint *textures ) \
	NGL( void, glDeleteTextures, GLsizei n, const GLuint *textures ) \
	NGL( void, glBindTexture, GLenum target, GLuint texture ) \
	NGL( void, glTexImage2D, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels ) \
	NGL( void, glTexParameteri, GLenum target, GLenum pname, GLint param ) \
	NGL( void, glTexParameterf, GLenum target, GLenum pname, GLfloat param ) \
	NGL( void, glCompressedTexImage2D,GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data ) \
	NGL( void, glCompressedTexSubImage2D, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data ) \
	NGL( void, glTexSubImage2D, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * data ) \
	NGL( void, glGenSamplers, GLsizei n, GLuint *samplers ) \
	NGL( void, glBindSampler, GLuint unit, GLuint sampler ) \
	NGL( void, glDeleteSamplers, GLsizei n, const GLuint *samplers ) \
	NGL( void, glSamplerParameteri, GLuint sampler, GLenum pname, GLint param ) \
	NGL( void, glSamplerParameterf, GLuint sampler, GLenum pname, GLfloat param ) \
	NGL( void, glGenerateMipmap, GLenum target ) \
	NGL( void, glBindImageTexture, GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format ) \
	NGL( void, glCopyTexSubImage2D, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height ) \
	NGL( void, glGetTexImage, GLenum target, GLint level, GLenum format, GLenum type, void *pixels ) \

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
	NGL( void, glVertexAttribPointerARB, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer ) \
	NGL( void, glGetVertexAttribivARB, GLuint index, GLenum pname, GLint *params ) \
	NGL( void, glGetVertexAttribPointervARB, GLuint index, GLenum pname, void **pointer )

#define NGL_VertexArray_Procs \
	NGL( void, glGenVertexArrays, GLsizei n, GLuint *arrays ) \
	NGL( void, glDeleteVertexArrays, GLsizei n, const GLuint *arrays ) \
	NGL( void, glBindVertexArray, GLuint array ) \
	NGL( void, glEnableVertexAttribArray, GLuint index ) \
	NGL( void, glDisableVertexAttribArray, GLuint index ) \
	NGL( void, glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer ) \
	NGL( void, glVertexAttribIPointer, GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer ) \
	NGL( void, glGetVertexAttribiv, GLuint index, GLenum pname, GLint *params ) \
	NGL( void, glGetVertexAttribPointerv, GLuint index, GLenum pname, void **pointer ) \
	NGL( void, glVertexAttribDivisor, GLuint index, GLuint divisor ) \

#define NGL_BufferARB_Procs \
	NGL( void, glGenBuffersARB, GLsizei n, GLuint *buffers ) \
	NGL( void, glDeleteBuffersARB, GLsizei n, const GLuint *buffers ) \
	NGL( void, glBindBufferARB, GLenum target, GLuint buffer ) \
	NGL( void, glBufferDataARB, GLenum target, GLsizeiptr size, const void *data, GLenum usage ) \
	NGL( void, glBufferSubDataARB, GLenum target, GLintptr offset, GLsizeiptr size, const void *data ) \
	NGL( void, glUnmapBufferARB, GLenum target ) \
	NGL( void*, glMapBufferARB, GLenum target, GLbitfield access ) \
	NGL( void, glBindBufferRangeARB, GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size ) \
	NGL( void, glBindBufferBaseARB, GLenum target, GLuint index, GLuint buffer )

#define NGL_Buffer_Procs \
	NGL( void, glGenBuffers, GLsizei n, GLuint *buffers ) \
	NGL( void, glDeleteBuffers, GLsizei n, const GLuint *buffers ) \
	NGL( void, glBindBuffer, GLenum target, GLuint buffer ) \
	NGL( void, glBufferData, GLenum target, GLsizeiptr size, const void *data, GLenum usage ) \
	NGL( void, glBufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const void *data ) \
	NGL( void*, glMapBuffer, GLenum target, GLbitfield access ) \
	NGL( void, glUnmapBuffer, GLenum target ) \
	NGL( void, glBindBufferRange, GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size ) \
	NGL( void, glBindBufferBase, GLenum target, GLuint index, GLuint buffer ) \
	NGL( void, glGetBufferParameteriv, GLenum target, GLenum value, GLint *data ) \

#define NGL_VertexShaderARB_Procs \
	NGL( void, glBindAttribLocationARB, GLhandleARB programObj, GLuint index, const GLcharARB *name) \
	NGL( void, glGetActiveAttribARB, GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name) \
	NGL( GLint, glGetAttribLocationARB, GLhandleARB programObj, const GLcharARB *name)

/*
* GL_EXT, GL_ARB procs
*/

#define NGL_ARB_direct_state_access \
	NGL( void, glCreateVertexArrays, GLsizei n, GLuint *vaos ) \
	NGL( void, glCreateBuffers, GLsizei n, GLuint *buffers ) \
	NGL( void, glNamedBufferData, GLuint buffer, GLsizeiptr size, const void *data, GLenum usage ) \
	NGL( void, glNamedBufferSubData, GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data ) \
	NGL( void*, glMapNamedBuffer, GLuint buffer, GLbitfield access ) \
	NGL( void, glUnmapNamedBuffer, GLuint buffer ) \
	NGL( void*, glMapNamedBufferRange, GLuint buffer, GLsizeiptr offset, GLsizeiptr length, GLbitfield access ) \
	NGL( void, glFlushMappedNamedBufferRange, GLuint buffer, GLintptr offset, GLsizeiptr length ) \
	NGL( void, glNamedBufferStorage, GLuint buffer, GLsizeiptr length, const void *data, GLbitfield access ) \
	NGL( void, glEnableVertexArrayAttrib, GLuint vaobj, GLuint index ) \
	NGL( void, glDisableVertexArrayAttrib, GLuint vaobj, GLuint index ) \
	NGL( void, glVertexArrayElementBuffer, GLuint vaobj, GLuint buffer ) \
	NGL( void, glVertexArrayVertexBuffer, GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride ) \
	NGL( void, glVertexArrayVertexBuffers, GLuint vaobj, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides ) \
	NGL( void, glVertexArrayAttribBinding, GLuint vaobj, GLuint attribindex, GLuint bindingindex ) \
	NGL( void, glVertexArrayAttribFormat, GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset ) \
	NGL( void, glVertexArrayAttribIFormat, GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset ) \
	NGL( void, glVertexArrayAttribLFormat, GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset ) \
	NGL( void, glVertexArrayBindingDivisor, GLuint vaobj, GLuint bindingindex, GLuint divisor ) \
	NGL( void, glBindVertexBuffer, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride ) \
	NGL( void, glCreateTextures, GLenum target, GLsizei n, GLuint *textures ) \
	NGL( void, glTextureStorage2D, GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height ) \
	NGL( void, glTextureSubImage2D, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels ) \
	NGL( void, glCompressedTextureSubImage2D, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data ) \
	NGL( void, glTextureParameterf, GLuint texture, GLenum pname, GLfloat param ) \
	NGL( void, glTextureParameterfv, GLuint texture, GLenum pname, const GLfloat *param ) \
	NGL( void, glTextureParameteri, GLuint texture, GLenum pname, GLint param ) \
	NGL( void, glTextureParameterIiv, GLuint texture, GLenum pname, const GLint *param ) \
	NGL( void, glBindTextureUnit, GLuint unit, GLuint texture ) \
	NGL( void, glCreateFramebuffers, GLsizei n, GLuint *framebuffers ) \
	NGL( void, glNamedFramebufferRenderbuffer, GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer ) \
	NGL( void, glNamedFramebufferTexture, GLuint framebuffer, GLenum attachment, GLuint texture, GLint level ) \
	NGL( void, glNamedFramebufferDrawBuffer, GLuint framebuffer, GLenum mode ) \
	NGL( void, glNamedFramebufferDrawBuffers, GLuint framebuffer, GLsizei n, const GLenum *bufs ) \
	NGL( void, glBlitNamedFramebuffer, GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter ) \
	NGL( GLenum, glCheckNamedFramebufferStatus, GLuint framebuffer, GLenum target ) \
	NGL( void, glCreateRenderbuffers, GLsizei n, GLuint *renderbuffers ) \
	NGL( void, glNamedRenderbufferStorage, GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height ) \
	NGL( void, glNamedRenderbufferStorageMultisample, GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height ) \

#define NGL_ARB_bindless_texture \
	NGL( GLuint64, glGetTextureHandleARB, GLuint texture ) \
	NGL( GLuint64, glGetTextureSamplerHandleARB, GLuint texture, GLuint sampler ) \
	NGL( void, glMakeTextureHandleResidentARB, GLuint64 handle ) \
	NGL( void, glMakeTextureHandleNonResidentARB, GLuint64 handle ) \
	NGL( GLboolean, glIsTextureHandleResidentARB, GLuint64 handle ) \
	NGL( void, glUniformHandleui64ARB, GLint location, GLuint64 value ) \
	NGL( void, glUniformHandleui64vARB, GLint location, GLsizei count, const GLuint64 *value )

#define NGL_ARB_transform_feedback \
	NGL( void, glGenTransformFeedback, GLsizei n, GLuint *ids ) \
	NGL( void, glDeleteTransformFeedbacks, GLsizei n, const GLuint *ids ) \
	NGL( void, glBindTransformFeedback, GLenum target, GLuint id ) \
	NGL( void, glBeginTransformFeedback, GLenum primitiveMode ) \
	NGL( void, glEndTransformFeedback, void ) \
	NGL( void, glPauseTransformFeedback, void ) \
	NGL( void, glTransformFeedbackVaryings, GLuint program, GLsizei count, const char **varyings, GLenum bufferMode ) \

#define NGL_NV_shader_buffer_load \
	NGL( void, glMakeBufferResidentNV, GLenum target, GLenum access ) \
	NGL( void, glMakeBufferNonResidentNV, GLenum target ) \
	NGL( void, glGetBufferParameterui64vNV, GLenum target, GLenum pname, GLuint64 *params )

#define NGL_ARB_sync \
	NGL( GLsync, glFenceSync, GLenum condition, GLbitfield flags ) \
	NGL( GLenum, glClientWaitSync, GLsync sync, GLbitfield flags, GLuint64 timeout ) \
	NGL( void, glWaitSync, GLsync sync, GLbitfield flags, GLuint64 timeout ) \
	NGL( void, glDeleteSync, GLsync sync )

#define NGL_ARB_map_buffer_range \
	NGL( void*, glMapBufferRange, GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access ) \
	NGL( void, glFlushMappedBufferRange, GLenum target, GLintptr offset, GLsizeiptr length ) \
	NGL( void, glInvalidateBufferData, GLuint buffer ) \
	NGL( void, glInvalidateBufferSubData, GLuint buffer, GLintptr offset, GLsizeiptr length )

#define NGL_ARB_buffer_storage \
	NGL( void, glBufferStorage, GLenum target, GLsizeiptr size, const void *data, GLbitfield flags )

#define NGL_ARB_shader_subroutine \
	NGL( GLuint, glGetSubroutineIndex, GLuint program, GLenum shaderType, const char *name ) \
	NGL( void, glGetProgramStageiv, GLuint program, GLenum shaderType, GLenum pname, GLint *values ) \
	NGL( void, glGetActiveSubroutineName, GLuint program, GLenum shaderType, GLuint index, GLsizei bufsize, GLsizei *length, char *name ) \
	NGL( void, glGetActiveSubroutineUniformiv, GLuint program, GLenum shaderType, GLuint index, GLenum pname, GLint *values ) \
	NGL( void, glGetSubroutineUniformLocation, GLuint program, GLenum shaderType, const char *name ) \
	NGL( void, glUniformSubroutinesuiv, GLenum shaderType, GLsizei count, const GLuint *indices ) \

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

#ifdef _NOMAD_DEBUG
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

NGL_ARB_transform_feedback
NGL_ARB_map_buffer_range
NGL_ARB_buffer_storage
NGL_ARB_sync
NGL_ARB_bindless_texture
NGL_ARB_direct_state_access
NGL_ARB_shader_subroutine
#undef NGL

#endif