#ifndef __IRENDER_PROGRAM_H__
#define __IRENDER_PROGRAM_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

namespace SIREngine::Rendering
{
	typedef enum shaderType : uint32_t
	{
		/// @brief 
		ShaderType_Vertex,

		/// @brief 
		ShaderType_Fragment,

		/// @brief 
		ShaderType_Geometry,

		/// @brief 
		ShaderType_Compute,

		/// @brief 
		ShaderType_TesselationControl,

		/// @brief 
		ShaderType_TesselationEvaluation,

		ShaderType_Count
	} EShaderType;

	typedef struct shaderModule_t
	{
		const char *pszName;
		void *pNativeHandle;
		EShaderType eType;
	} SShaderModule;

	class IRenderProgram
	{
	public:
		virtual ~IRenderProgram() = default;

		virtual void Compile( void ) = 0;

		const SShaderModule *GetModule( EShaderType eType ) const;
	protected:
		SShaderModule m_arrModules[ ShaderType_Count ];

		const char *m_pszName;
	};

	SIRENGINE_FORCEINLINE const SShaderModule *IRenderProgram::GetModule( EShaderType eType ) const
	{
		return &m_arrModules[ eType ];
	}
};

#endif