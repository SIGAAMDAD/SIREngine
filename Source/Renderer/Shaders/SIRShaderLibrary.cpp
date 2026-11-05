/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Shaders/SIRShaderLibrary.h>

SIRResourceResult sirShaderResolveMaterialStage( const sirShaderLibraryContext_t& context, const sirShaderMaterialProjection_t& material,
                                                 const sirShaderResolveStageRequest_t& request, sirShaderResolvedStage_t *pOut ) noexcept
{
	if ( pOut == nullptr || context.pGraph == nullptr || context.pScope == nullptr || !context.pScope->isActive() ||
	     !material.view.isValid() || request.nStageMask == 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	sirShaderMaterialPassRecord_t pass = {};
	SIRResourceResult eResult = sirShaderProjectMaterialPass( material, request.ePass, &pass );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	if ( pass.iShaderDependency == SIR_SHADER_MATERIAL_NO_SHADER ) {
		if ( ( pass.nRequiredFeatures & SIR_GRAPHICS_FEATURE_FIXED_FUNCTION ) == 0 ||
		     ( request.validation.nAvailableFeatures & pass.nRequiredFeatures ) != pass.nRequiredFeatures ) {
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		sirShaderResolvedStage_t fixed = {};
		fixed.material = pass;
		fixed.nMaterialGraphRevision = material.nGraphRevision;
		fixed.bFixedFunction = true;
		*pOut = fixed;
		return SIR_RESOURCE_RESULT_SUCCESS;
	}
	if ( ( request.validation.nAvailableFeatures & pass.nRequiredFeatures ) != pass.nRequiredFeatures ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	SIRResourceDependency dependency = {};
	eResult = material.view.getDependency( pass.iShaderDependency, &dependency );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	if ( dependency.m_eKind != SIR_RESOURCE_EDGE_KIND_REQUIRED || dependency.m_Target.m_nTypeId != context.formats.nShaderTypeId ) {
		return SIR_RESOURCE_RESULT_TYPE_MISMATCH;
	}
	SIRResourceView source;
	eResult = context.pGraph->resolveDependency( *context.pScope, material.view, pass.iShaderDependency, &source );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	sirShaderPackageProjection_t package = {};
	eResult = sirShaderProjectPackage( source, context.formats, request.validation, &package );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	for ( uint32_t iStage = 0; iStage < package.header.nStageCount; ++iStage ) {
		sirShaderStageProjection_t candidate = {};
		eResult = sirShaderProjectStage( package, iStage, &candidate );
		if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResult;
		}
		if ( candidate.stage.ePass != pass.nPass || candidate.stage.nStageMask != request.nStageMask ) {
			continue;
		}
		sirShaderResolvedStage_t resolved = {};
		resolved.material = pass;
		resolved.stage = candidate.stage;
		resolved.hShader = source.getHandle();
		resolved.binary = candidate.binary;
		resolved.nMaterialGraphRevision = material.nGraphRevision;
		resolved.nShaderGraphRevision = package.nGraphRevision;
		resolved.nShaderSourceRevision = package.header.nSourceRevision;
		*pOut = resolved;
		return SIR_RESOURCE_RESULT_SUCCESS;
	}
	return SIR_RESOURCE_RESULT_DEPENDENCY_MISSING;
}
