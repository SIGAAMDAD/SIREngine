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

#include <Scripting/Language/SIRLangGraphVM.h>
#include <limits.h>
#include <string.h>

static bool sirLangIsInteger( sirLangValueKind_t eType ) noexcept
{
	return eType >= SIR_LANG_VALUE_I8 && eType <= SIR_LANG_VALUE_U64;
}

static bool sirLangIsSignedInteger( sirLangValueKind_t eType ) noexcept
{
	return eType == SIR_LANG_VALUE_I8 || eType == SIR_LANG_VALUE_I16 || eType == SIR_LANG_VALUE_I32 || eType == SIR_LANG_VALUE_I64;
}

static uint32_t sirLangIntegerWidth( sirLangValueKind_t eType ) noexcept
{
	switch ( eType ) {
	case SIR_LANG_VALUE_I8:
	case SIR_LANG_VALUE_U8:
		return 8;
	case SIR_LANG_VALUE_I16:
	case SIR_LANG_VALUE_U16:
		return 16;
	case SIR_LANG_VALUE_I32:
	case SIR_LANG_VALUE_U32:
		return 32;
	case SIR_LANG_VALUE_I64:
	case SIR_LANG_VALUE_U64:
		return 64;
	default:
		return 0;
	}
}

static int64_t sirLangReadSigned( uint64_t nBits, uint32_t nWidth ) noexcept
{
	if ( nWidth == 64 ) {
		int64_t nValue = 0;
		memcpy( &nValue, &nBits, sizeof( nValue ) );
		return nValue;
	}
	const uint64_t nMask = ( UINT64_C( 1 ) << nWidth ) - 1;
	const int64_t nValue = ( int64_t )( nBits & nMask );
	if ( nBits & ( UINT64_C( 1 ) << ( nWidth - 1 ) ) ) {
		return nValue - ( int64_t )( UINT64_C( 1 ) << nWidth );
	}
	return nValue;
}

static bool sirLangFitsWidth( uint64_t nBits, sirLangValueKind_t eType ) noexcept
{
	if ( eType == SIR_LANG_VALUE_BOOL ) {
		return nBits <= 1;
	}
	const uint32_t nWidth = sirLangIntegerWidth( eType );
	if ( nWidth == 0 || nWidth == 64 ) {
		return true;
	}
	const uint64_t nMask = ( UINT64_C( 1 ) << nWidth ) - 1;
	if ( sirLangIsSignedInteger( eType ) ) {
		const uint64_t nSign = UINT64_C( 1 ) << ( nWidth - 1 );
		const uint64_t nUpper = nBits & ~nMask;
		return ( nUpper == 0 && ( nBits & nSign ) == 0 ) || ( nUpper == ~nMask && ( nBits & nSign ) != 0 );
	}
	return ( nBits & ~nMask ) == 0;
}

static bool sirLangValidRuntimeType( sirLangValueKind_t eType ) noexcept
{
	return eType == SIR_LANG_VALUE_BOOL || sirLangIsInteger( eType ) || eType == SIR_LANG_VALUE_F32 || eType == SIR_LANG_VALUE_F64;
}

static bool sirLangValidRuntimeValue( const sirLangVMValue_t& value ) noexcept
{
	if ( !sirLangValidRuntimeType( value.eType ) ) {
		return false;
	}
	if ( value.eType == SIR_LANG_VALUE_F32 ) {
		return ( value.nBits >> 32 ) == 0;
	}
	if ( value.eType == SIR_LANG_VALUE_F64 ) {
		return true;
	}
	return sirLangFitsWidth( value.nBits, value.eType );
}

static bool sirLangWithinFunction( const sirLangCIRFunction_t& function, uint32_t iInstruction ) noexcept
{
	return iInstruction >= function.iFirstInstruction && iInstruction - function.iFirstInstruction < function.nInstructionCount;
}

static bool sirLangIsDefinition( sirLangOpcode_t eOpcode ) noexcept
{
	return eOpcode != SIR_LANG_OP_JUMP && eOpcode != SIR_LANG_OP_JUMP_FALSE && eOpcode != SIR_LANG_OP_RETURN;
}

typedef struct sirLangVerifierRegisters_s
{
	const sirLangValueKind_t *pRegisterTypes;
	uint64_t nDefinitions;
	uint32_t nRegisterCount;
} sirLangVerifierRegisters_t;

static bool sirLangUseRegister( uint32_t iRegister, sirLangValueKind_t eExpected, const sirLangVerifierRegisters_t& registers ) noexcept
{
	return iRegister < registers.nRegisterCount && ( registers.nDefinitions & ( UINT64_C( 1 ) << iRegister ) ) != 0 &&
	       ( eExpected == SIR_LANG_VALUE_UNKNOWN || registers.pRegisterTypes[iRegister] == eExpected );
}

static bool sirLangValidParameterSpan( uint32_t iFirst, uint32_t nCount, uint32_t nTotal ) noexcept
{
	return iFirst <= nTotal && nCount <= nTotal - iFirst;
}

static sirLangVMError_t sirLangVerifyInstruction( const sirLangCIRProgram_t *pProgram, const sirLangCIRFunction_t& function,
                                                  const sirLangCIRInstruction_t& instruction,
                                                  const sirLangVerifierRegisters_t& registers ) noexcept
{
	if ( instruction.eOpcode > SIR_LANG_OP_PHI ) {
		return SIR_LANG_VM_INVALID_OPCODE;
	}
	const bool bDefinition = sirLangIsDefinition( instruction.eOpcode );
	if ( bDefinition && ( instruction.iDestination >= function.nRegisterCount || !sirLangValidRuntimeType( instruction.eType ) ) ) {
		return SIR_LANG_VM_INVALID_REGISTER;
	}
	if ( !bDefinition && instruction.iDestination != UINT32_MAX ) {
		return SIR_LANG_VM_INVALID_REGISTER;
	}
	const uint32_t iA = instruction.iFirstSource;
	const uint32_t iB = instruction.iSecondSource;
	const sirLangValueKind_t eType = instruction.eType;
	const bool bFirstValid = sirLangUseRegister( iA, SIR_LANG_VALUE_UNKNOWN, registers );
	const bool bSecondValid = sirLangUseRegister( iB, SIR_LANG_VALUE_UNKNOWN, registers );
	switch ( instruction.eOpcode ) {
	case SIR_LANG_OP_CONST:
		if ( !sirLangValidRuntimeValue( { instruction.nImmediate, eType } ) ) {
			return SIR_LANG_VM_INVALID_TYPE;
		}
		return SIR_LANG_VM_OK;
	case SIR_LANG_OP_PARAMETER:
		if ( iA >= function.nParameterCount || pProgram->pParameterTypes[function.iFirstParameterType + iA] != eType ) {
			return SIR_LANG_VM_INVALID_TYPE;
		}
		return SIR_LANG_VM_OK;
	case SIR_LANG_OP_MOVE:
		return bFirstValid && registers.pRegisterTypes[iA] == eType ? SIR_LANG_VM_OK : SIR_LANG_VM_INVALID_REGISTER;
	case SIR_LANG_OP_ADD:
	case SIR_LANG_OP_SUBTRACT:
	case SIR_LANG_OP_MULTIPLY:
	case SIR_LANG_OP_DIVIDE:
		return bFirstValid && bSecondValid && sirLangIsInteger( eType ) && registers.pRegisterTypes[iA] == eType &&
		               registers.pRegisterTypes[iB] == eType
		           ? SIR_LANG_VM_OK
				   : SIR_LANG_VM_INVALID_TYPE;
	case SIR_LANG_OP_EQUAL:
	case SIR_LANG_OP_LESS:
	case SIR_LANG_OP_LESS_EQUAL:
		return bFirstValid && bSecondValid && eType == SIR_LANG_VALUE_BOOL &&
		               ( sirLangIsInteger( registers.pRegisterTypes[iA] ) || registers.pRegisterTypes[iA] == SIR_LANG_VALUE_BOOL ) &&
		               registers.pRegisterTypes[iA] == registers.pRegisterTypes[iB]
		           ? SIR_LANG_VM_OK
				   : SIR_LANG_VM_INVALID_TYPE;
	case SIR_LANG_OP_NOT:
		return bFirstValid && registers.pRegisterTypes[iA] == SIR_LANG_VALUE_BOOL && eType == SIR_LANG_VALUE_BOOL
		           ? SIR_LANG_VM_OK
				   : SIR_LANG_VM_INVALID_TYPE;
	case SIR_LANG_OP_AND:
	case SIR_LANG_OP_OR:
		return bFirstValid && bSecondValid && registers.pRegisterTypes[iA] == SIR_LANG_VALUE_BOOL &&
		               registers.pRegisterTypes[iB] == SIR_LANG_VALUE_BOOL && eType == SIR_LANG_VALUE_BOOL
		           ? SIR_LANG_VM_OK
				   : SIR_LANG_VM_INVALID_TYPE;
	case SIR_LANG_OP_JUMP:
		return sirLangWithinFunction( function, instruction.nAuxiliary ) ? SIR_LANG_VM_OK : SIR_LANG_VM_INVALID_CONTROL_FLOW;
	case SIR_LANG_OP_JUMP_FALSE:
		return bFirstValid && registers.pRegisterTypes[iA] == SIR_LANG_VALUE_BOOL &&
		               sirLangWithinFunction( function, instruction.nAuxiliary )
		           ? SIR_LANG_VM_OK
				   : SIR_LANG_VM_INVALID_CONTROL_FLOW;
	case SIR_LANG_OP_RETURN:
		if ( function.eReturnType == SIR_LANG_VALUE_VOID ) {
			return iA == UINT32_MAX ? SIR_LANG_VM_OK : SIR_LANG_VM_INVALID_TYPE;
		}
		return bFirstValid && registers.pRegisterTypes[iA] == function.eReturnType ? SIR_LANG_VM_OK : SIR_LANG_VM_INVALID_TYPE;
	case SIR_LANG_OP_CALL:
	case SIR_LANG_OP_CALL_NATIVE: {
		const bool bNative = instruction.eOpcode == SIR_LANG_OP_CALL_NATIVE;
		if ( iA >= ( bNative ? pProgram->nCapabilityCount : pProgram->nFunctionCount ) ||
		     !sirLangValidParameterSpan( instruction.nAuxiliary, iB, pProgram->nCallArgumentCount ) ) {
			return SIR_LANG_VM_INVALID_CALL;
		}
		const sirLangCIRFunction_t *pFunction = bNative ? nullptr : &pProgram->pFunctions[iA];
		const sirLangCIRCapability_t *pCapability = bNative ? &pProgram->pCapabilities[iA] : nullptr;
		const uint32_t nExpected = bNative ? pCapability->nParameterCount : pFunction->nParameterCount;
		const uint32_t iExpected = bNative ? pCapability->iFirstParameterType : pFunction->iFirstParameterType;
		const sirLangValueKind_t eExpected = bNative ? pCapability->eReturnType : pFunction->eReturnType;
		if ( iB != nExpected || eType != eExpected || eExpected == SIR_LANG_VALUE_VOID ) {
			return SIR_LANG_VM_INVALID_CALL;
		}
		for ( uint32_t iArg = 0; iArg < iB; ++iArg ) {
			const uint32_t iReg = pProgram->pCallArguments[instruction.nAuxiliary + iArg];
			if ( !sirLangUseRegister( iReg, pProgram->pParameterTypes[iExpected + iArg], registers ) ) {
				return SIR_LANG_VM_INVALID_CALL;
			}
		}
		return SIR_LANG_VM_OK;
	}
	case SIR_LANG_OP_PHI:
		return bFirstValid && bSecondValid && registers.pRegisterTypes[iA] == eType && registers.pRegisterTypes[iB] == eType
		           ? SIR_LANG_VM_OK
				   : SIR_LANG_VM_INVALID_TYPE;
	default:
		return SIR_LANG_VM_INVALID_OPCODE;
	}
}

sirLangVMError_t SIRLangVerifyCIR( const sirLangCIRProgram_t *pProgram ) noexcept
{
	if ( pProgram == nullptr || pProgram->pFunctions == nullptr || pProgram->pInstructions == nullptr ||
	     pProgram->pParameterTypes == nullptr || ( pProgram->nCallArgumentCount > 0 && pProgram->pCallArguments == nullptr ) ||
	     ( pProgram->nCapabilityCount > 0 && pProgram->pCapabilities == nullptr ) || pProgram->nFunctionCount == 0 ||
	     pProgram->nRevision != SIR_LANG_VM_VERSION || pProgram->nLayoutDigest == 0 || pProgram->nBindingDigest == 0 ) {
		return SIR_LANG_VM_INVALID_FORMAT;
	}
	for ( uint32_t iCapability = 0; iCapability < pProgram->nCapabilityCount; ++iCapability ) {
		const sirLangCIRCapability_t& capability = pProgram->pCapabilities[iCapability];
		if ( capability.nFunctionId == 0 || capability.nSignatureId == 0 || capability.nBindingRevision == 0 ||
		     capability.nEffects & ~UINT32_C( 127 ) || capability.nParameterCount > SIR_LANG_VM_MAX_PARAMETERS ||
		     !sirLangValidParameterSpan( capability.iFirstParameterType, capability.nParameterCount, pProgram->nParameterTypeCount ) ) {
			return SIR_LANG_VM_INVALID_CAPABILITY;
		}
	}
	// Prevalidate every function before call-instruction validation references
	// the callee's parameter span (including forward calls).
	for ( uint32_t iFunction = 0; iFunction < pProgram->nFunctionCount; ++iFunction ) {
		const sirLangCIRFunction_t& function = pProgram->pFunctions[iFunction];
		if ( function.nFunctionId == 0 || function.nRegisterCount == 0 || function.nRegisterCount > SIR_LANG_VM_MAX_REGISTERS ||
		     function.nInstructionCount == 0 || function.nParameterCount > SIR_LANG_VM_MAX_PARAMETERS ||
		     !sirLangValidParameterSpan( function.iFirstInstruction, function.nInstructionCount, pProgram->nInstructionCount ) ||
		     !sirLangValidParameterSpan( function.iFirstParameterType, function.nParameterCount, pProgram->nParameterTypeCount ) ) {
			return SIR_LANG_VM_INVALID_FORMAT;
		}
		for ( uint32_t iParam = 0; iParam < function.nParameterCount; ++iParam ) {
			if ( !sirLangValidRuntimeType( pProgram->pParameterTypes[function.iFirstParameterType + iParam] ) ) {
				return SIR_LANG_VM_INVALID_TYPE;
			}
		}
		for ( uint32_t jFunction = 0; jFunction < iFunction; ++jFunction ) {
			if ( pProgram->pFunctions[jFunction].nFunctionId == function.nFunctionId ) {
				return SIR_LANG_VM_INVALID_FORMAT;
			}
		}
		for ( uint32_t jFunction = 0; jFunction < iFunction; ++jFunction ) {
			const sirLangCIRFunction_t& previous = pProgram->pFunctions[jFunction];
			const uint32_t nEnd = previous.iFirstInstruction + previous.nInstructionCount;
			const uint32_t nThisEnd = function.iFirstInstruction + function.nInstructionCount;
			if ( function.iFirstInstruction < nEnd && previous.iFirstInstruction < nThisEnd ) {
				return SIR_LANG_VM_INVALID_FORMAT;
			}
		}
	}
	for ( uint32_t iFunction = 0; iFunction < pProgram->nFunctionCount; ++iFunction ) {
		const sirLangCIRFunction_t& function = pProgram->pFunctions[iFunction];
		if ( function.nFunctionId == 0 || function.nRegisterCount == 0 || function.nRegisterCount > SIR_LANG_VM_MAX_REGISTERS ||
		     function.nInstructionCount == 0 || function.nParameterCount > SIR_LANG_VM_MAX_PARAMETERS ||
		     !sirLangValidParameterSpan( function.iFirstInstruction, function.nInstructionCount, pProgram->nInstructionCount ) ||
		     !sirLangValidParameterSpan( function.iFirstParameterType, function.nParameterCount, pProgram->nParameterTypeCount ) ) {
			return SIR_LANG_VM_INVALID_FORMAT;
		}
		for ( uint32_t jFunction = 0; jFunction < iFunction; ++jFunction ) {
			if ( pProgram->pFunctions[jFunction].nFunctionId == function.nFunctionId ) {
				return SIR_LANG_VM_INVALID_FORMAT;
			}
		}
		sirLangValueKind_t szRegTypes[SIR_LANG_VM_MAX_REGISTERS] = {};
		uint64_t nDefinitions = 0;
		for ( uint32_t i = function.iFirstInstruction; i < function.iFirstInstruction + function.nInstructionCount; ++i ) {
			const sirLangCIRInstruction_t& instruction = pProgram->pInstructions[i];
			const sirLangVerifierRegisters_t registers = { szRegTypes, nDefinitions, function.nRegisterCount };
			const sirLangVMError_t eError = sirLangVerifyInstruction( pProgram, function, instruction, registers );
			if ( eError != SIR_LANG_VM_OK ) {
				return eError;
			}
			if ( sirLangIsDefinition( instruction.eOpcode ) ) {
				const uint64_t nBit = UINT64_C( 1 ) << instruction.iDestination;
				if ( nDefinitions & nBit ) {
					return SIR_LANG_VM_INVALID_REGISTER;
				}
				nDefinitions |= nBit;
				szRegTypes[instruction.iDestination] = instruction.eType;
			}
		}
		if ( pProgram->pInstructions[function.iFirstInstruction + function.nInstructionCount - 1].eOpcode != SIR_LANG_OP_RETURN &&
		     pProgram->pInstructions[function.iFirstInstruction + function.nInstructionCount - 1].eOpcode != SIR_LANG_OP_JUMP ) {
			return SIR_LANG_VM_INVALID_CONTROL_FLOW;
		}
		// Meet-over-all-predecessors dataflow: a register use must be defined on
		// every route reaching its instruction. A purely lexical check is unsafe.
		if ( function.nInstructionCount > 1024 ) {
			return SIR_LANG_VM_CAPACITY;
		}
		bool szReachable[1024] = {};
		uint64_t szMustDefine[1024] = {};
		for ( uint32_t i = 0; i < function.nInstructionCount; ++i ) {
			szMustDefine[i] = UINT64_MAX;
		}
		szReachable[0] = true;
		szMustDefine[0] = 0;
		for ( uint32_t nPass = 0; nPass < function.nInstructionCount; ++nPass ) {
			bool bChanged = false;
			for ( uint32_t i = 0; i < function.nInstructionCount; ++i ) {
				if ( !szReachable[i] ) {
					continue;
				}
				const sirLangCIRInstruction_t& ins = pProgram->pInstructions[function.iFirstInstruction + i];
				uint64_t nOut = szMustDefine[i];
				if ( sirLangIsDefinition( ins.eOpcode ) ) {
					nOut |= UINT64_C( 1 ) << ins.iDestination;
				}
				uint32_t szSuccessors[2] = {};
				uint32_t nSuccessors = 0;
				if ( ins.eOpcode == SIR_LANG_OP_JUMP || ins.eOpcode == SIR_LANG_OP_JUMP_FALSE ) {
					szSuccessors[nSuccessors++] = ins.nAuxiliary - function.iFirstInstruction;
				}
				if ( ins.eOpcode != SIR_LANG_OP_JUMP && ins.eOpcode != SIR_LANG_OP_RETURN ) {
					if ( i + 1 >= function.nInstructionCount ) {
						return SIR_LANG_VM_INVALID_CONTROL_FLOW;
					}
					szSuccessors[nSuccessors++] = i + 1;
				}
				for ( uint32_t j = 0; j < nSuccessors; ++j ) {
					const uint32_t iNext = szSuccessors[j];
					if ( !szReachable[iNext] ) {
						szReachable[iNext] = true;
						szMustDefine[iNext] = nOut;
						bChanged = true;
					} else {
						const uint64_t nBefore = szMustDefine[iNext];
						szMustDefine[iNext] &= nOut;
						bChanged |= nBefore != szMustDefine[iNext];
					}
				}
			}
			if ( !bChanged ) {
				break;
			}
		}
		for ( uint32_t i = 0; i < function.nInstructionCount; ++i ) {
			if ( !szReachable[i] ) {
				return SIR_LANG_VM_INVALID_CONTROL_FLOW;
			}
			const sirLangCIRInstruction_t& ins = pProgram->pInstructions[function.iFirstInstruction + i];
			const uint64_t nAvailable = szMustDefine[i];
			if ( ins.eOpcode == SIR_LANG_OP_PHI ) {
				uint32_t iJoin = i;
				while ( iJoin > 0 && pProgram->pInstructions[function.iFirstInstruction + iJoin - 1].eOpcode == SIR_LANG_OP_PHI ) {
					--iJoin;
				}
				if ( ins.nAuxiliary < function.iFirstInstruction || ins.nAuxiliary >= function.iFirstInstruction + iJoin ) {
					return SIR_LANG_VM_INVALID_CONTROL_FLOW;
				}
				const sirLangCIRInstruction_t& thenJump = pProgram->pInstructions[ins.nAuxiliary];
				if ( thenJump.eOpcode != SIR_LANG_OP_JUMP || thenJump.nAuxiliary != function.iFirstInstruction + iJoin ) {
					return SIR_LANG_VM_INVALID_CONTROL_FLOW;
				}
				bool bFoundThen = false;
				bool bFoundElse = false;
				for ( uint32_t iPred = 0; iPred < function.nInstructionCount; ++iPred ) {
					if ( !szReachable[iPred] ) {
						continue;
					}
					const sirLangCIRInstruction_t& pred = pProgram->pInstructions[function.iFirstInstruction + iPred];
					const bool bExplicit = ( pred.eOpcode == SIR_LANG_OP_JUMP || pred.eOpcode == SIR_LANG_OP_JUMP_FALSE ) &&
					                       pred.nAuxiliary == function.iFirstInstruction + iJoin;
					const bool bFallthrough = iPred + 1 == iJoin && pred.eOpcode != SIR_LANG_OP_JUMP && pred.eOpcode != SIR_LANG_OP_RETURN;
					if ( !bExplicit && !bFallthrough ) {
						continue;
					}
					const bool bThen = iPred + function.iFirstInstruction == ins.nAuxiliary;
					const uint32_t iSource = bThen ? ins.iFirstSource : ins.iSecondSource;
					uint64_t nOut = szMustDefine[iPred];
					if ( sirLangIsDefinition( pred.eOpcode ) ) {
						nOut |= UINT64_C( 1 ) << pred.iDestination;
					}
					if ( !( nOut & ( UINT64_C( 1 ) << iSource ) ) ) {
						return SIR_LANG_VM_UNINITIALIZED;
					}
					bFoundThen |= bThen;
					bFoundElse |= !bThen;
				}
				if ( !bFoundThen || !bFoundElse ) {
					return SIR_LANG_VM_INVALID_CONTROL_FLOW;
				}
				continue;
			}
			switch ( ins.eOpcode ) {
			case SIR_LANG_OP_MOVE:
			case SIR_LANG_OP_NOT:
			case SIR_LANG_OP_JUMP_FALSE:
				if ( !( nAvailable & ( UINT64_C( 1 ) << ins.iFirstSource ) ) ) {
					return SIR_LANG_VM_UNINITIALIZED;
				}
				break;
			case SIR_LANG_OP_ADD:
			case SIR_LANG_OP_SUBTRACT:
			case SIR_LANG_OP_MULTIPLY:
			case SIR_LANG_OP_DIVIDE:
			case SIR_LANG_OP_EQUAL:
			case SIR_LANG_OP_LESS:
			case SIR_LANG_OP_LESS_EQUAL:
			case SIR_LANG_OP_AND:
			case SIR_LANG_OP_OR:
				if ( !( nAvailable & ( UINT64_C( 1 ) << ins.iFirstSource ) ) || !( nAvailable & ( UINT64_C( 1 ) << ins.iSecondSource ) ) ) {
					return SIR_LANG_VM_UNINITIALIZED;
				}
				break;
			case SIR_LANG_OP_RETURN:
				if ( function.eReturnType != SIR_LANG_VALUE_VOID && !( nAvailable & ( UINT64_C( 1 ) << ins.iFirstSource ) ) ) {
					return SIR_LANG_VM_UNINITIALIZED;
				}
				break;
			case SIR_LANG_OP_CALL:
			case SIR_LANG_OP_CALL_NATIVE:
				for ( uint32_t iArg = 0; iArg < ins.iSecondSource; ++iArg ) {
					const uint32_t iReg = pProgram->pCallArguments[ins.nAuxiliary + iArg];
					if ( !( nAvailable & ( UINT64_C( 1 ) << iReg ) ) ) {
						return SIR_LANG_VM_UNINITIALIZED;
					}
				}
				break;
			default:
				break;
			}
		}
	}
	return SIR_LANG_VM_OK;
}

static sirLangVMError_t sirLangIntegerOperation( const sirLangCIRInstruction_t& instruction, sirLangVMValue_t first,
                                                 sirLangVMValue_t second, sirLangVMValue_t *pResult ) noexcept
{
	const uint32_t nWidth = sirLangIntegerWidth( instruction.eType );
	const bool bSigned = sirLangIsSignedInteger( instruction.eType );
	const uint64_t nMask = nWidth == 64 ? UINT64_MAX : ( UINT64_C( 1 ) << nWidth ) - 1;
	uint64_t nRaw = 0;
	if ( bSigned ) {
		const int64_t nA = sirLangReadSigned( first.nBits, nWidth );
		const int64_t nB = sirLangReadSigned( second.nBits, nWidth );
		int64_t nResult = 0;
		bool bOverflow = false;
		switch ( instruction.eOpcode ) {
		case SIR_LANG_OP_ADD:
			bOverflow = ( nB > 0 && nA > INT64_MAX - nB ) || ( nB < 0 && nA < INT64_MIN - nB );
			if ( !bOverflow ) {
				nResult = nA + nB;
			}
			break;
		case SIR_LANG_OP_SUBTRACT:
			bOverflow = ( nB < 0 && nA > INT64_MAX + nB ) || ( nB > 0 && nA < INT64_MIN + nB );
			if ( !bOverflow ) {
				nResult = nA - nB;
			}
			break;
		case SIR_LANG_OP_MULTIPLY:
			if ( nA > 0 ) {
				bOverflow = ( nB > 0 && nA > INT64_MAX / nB ) || ( nB < 0 && nB < INT64_MIN / nA );
			} else if ( nA < 0 ) {
				bOverflow = ( nB > 0 && nA < INT64_MIN / nB ) || ( nB < 0 && nA < INT64_MAX / nB );
			}
			if ( !bOverflow ) {
				nResult = nA * nB;
			}
			break;
		case SIR_LANG_OP_DIVIDE:
			if ( nB == 0 ) {
				return SIR_LANG_VM_DIVIDE_ZERO;
			}
			if ( nA == INT64_MIN && nB == -1 ) {
				return SIR_LANG_VM_ARITHMETIC_OVERFLOW;
			}
			nResult = nA / nB;
			break;
		default:
			return SIR_LANG_VM_INVALID_OPCODE;
		}
		if ( bOverflow ) {
			return SIR_LANG_VM_ARITHMETIC_OVERFLOW;
		}
		if ( nWidth < 64 ) {
			const int64_t nMin = -( INT64_C( 1 ) << ( nWidth - 1 ) );
			const int64_t nMax = ( INT64_C( 1 ) << ( nWidth - 1 ) ) - 1;
			if ( nResult < nMin || nResult > nMax ) {
				return SIR_LANG_VM_ARITHMETIC_OVERFLOW;
			}
		}
		memcpy( &nRaw, &nResult, sizeof( nRaw ) );
	} else {
		const uint64_t nA = first.nBits & nMask;
		const uint64_t nB = second.nBits & nMask;
		bool bOverflow = false;
		switch ( instruction.eOpcode ) {
		case SIR_LANG_OP_ADD:
			bOverflow = nA > UINT64_MAX - nB;
			if ( !bOverflow ) {
				nRaw = nA + nB;
			}
			break;
		case SIR_LANG_OP_SUBTRACT:
			bOverflow = nA < nB;
			if ( !bOverflow ) {
				nRaw = nA - nB;
			}
			break;
		case SIR_LANG_OP_MULTIPLY:
			bOverflow = nB != 0 && nA > UINT64_MAX / nB;
			if ( !bOverflow ) {
				nRaw = nA * nB;
			}
			break;
		case SIR_LANG_OP_DIVIDE:
			if ( nB == 0 ) {
				return SIR_LANG_VM_DIVIDE_ZERO;
			}
			nRaw = nA / nB;
			break;
		default:
			return SIR_LANG_VM_INVALID_OPCODE;
		}
		if ( bOverflow || ( nRaw & ~nMask ) != 0 ) {
			return SIR_LANG_VM_ARITHMETIC_OVERFLOW;
		}
	}
	*pResult = { nRaw, instruction.eType };
	return SIR_LANG_VM_OK;
}

typedef struct sirLangVMFrame_s
{
	sirLangVMValue_t szRegisters[SIR_LANG_VM_MAX_REGISTERS];
	sirLangVMValue_t szParameters[SIR_LANG_VM_MAX_PARAMETERS];
	uint64_t nInitialized;
	uint32_t iFunction;
	uint32_t iInstruction;
	uint32_t iReturnRegister;
	uint32_t iPreviousInstruction;
	uint32_t iPhiPredecessor;
} sirLangVMFrame_t;

sirLangVMRunResult_t SIRLangRunGraphVM( const sirLangCIRProgram_t *pProgram, const sirLangVMInvocation_t& invocation ) noexcept
{
	sirLangVMRunResult_t output = {};
	output.eError = SIRLangVerifyCIR( pProgram );
	if ( output.eError != SIR_LANG_VM_OK ) {
		return output;
	}
	if ( invocation.nExpectedLayoutDigest != pProgram->nLayoutDigest || invocation.nExpectedBindingDigest != pProgram->nBindingDigest ||
	     invocation.nFuel == 0 ) {
		output.eError = SIR_LANG_VM_INVALID_FORMAT;
		return output;
	}
	uint32_t iTargetFunction = UINT32_MAX;
	for ( uint32_t iFunction = 0; iFunction < pProgram->nFunctionCount; ++iFunction ) {
		if ( pProgram->pFunctions[iFunction].nFunctionId == invocation.nFunctionId ) {
			iTargetFunction = iFunction;
			break;
		}
	}
	if ( iTargetFunction == UINT32_MAX || invocation.nArgumentCount != pProgram->pFunctions[iTargetFunction].nParameterCount ||
	     ( invocation.nArgumentCount != 0 && invocation.pArguments == nullptr ) ) {
		output.eError = SIR_LANG_VM_INVALID_ARGUMENT;
		return output;
	}
	for ( uint32_t i = 0; i < invocation.nArgumentCount; ++i ) {
		const sirLangValueKind_t eExpected = pProgram->pParameterTypes[pProgram->pFunctions[iTargetFunction].iFirstParameterType + i];
		if ( invocation.pArguments[i].eType != eExpected || !sirLangValidRuntimeValue( invocation.pArguments[i] ) ) {
			output.eError = SIR_LANG_VM_INVALID_TYPE;
			return output;
		}
	}
	sirLangVMFrame_t szFrames[SIR_LANG_VM_MAX_FRAMES] = {};
	uint32_t nFrames = 1;
	szFrames[0].iFunction = iTargetFunction;
	szFrames[0].iInstruction = pProgram->pFunctions[iTargetFunction].iFirstInstruction;
	for ( uint32_t i = 0; i < invocation.nArgumentCount; ++i ) {
		szFrames[0].szParameters[i] = invocation.pArguments[i];
	}
	while ( nFrames > 0 && output.nFuelConsumed < invocation.nFuel ) {
		sirLangVMFrame_t& frame = szFrames[nFrames - 1];
		const sirLangCIRFunction_t& function = pProgram->pFunctions[frame.iFunction];
		if ( !sirLangWithinFunction( function, frame.iInstruction ) ) {
			output.eError = SIR_LANG_VM_INVALID_CONTROL_FLOW;
			return output;
		}
		const uint32_t iExecuting = frame.iInstruction;
		const sirLangCIRInstruction_t& ins = pProgram->pInstructions[iExecuting];
		if ( ins.eOpcode == SIR_LANG_OP_PHI ) {
			if ( frame.iPreviousInstruction >= pProgram->nInstructionCount ||
			     pProgram->pInstructions[frame.iPreviousInstruction].eOpcode != SIR_LANG_OP_PHI ) {
				frame.iPhiPredecessor = frame.iPreviousInstruction;
			}
		}
		frame.iPreviousInstruction = iExecuting;
		output.nInstruction = iExecuting;
		output.nSourceNode = ins.iSourceNode;
		++output.nFuelConsumed;
		++frame.iInstruction;
		sirLangVMValue_t value = {};
		const sirLangVMValue_t first = ins.iFirstSource < function.nRegisterCount ? frame.szRegisters[ins.iFirstSource] : value;
		const sirLangVMValue_t second = ins.iSecondSource < function.nRegisterCount ? frame.szRegisters[ins.iSecondSource] : value;
		bool bWritesDestination = sirLangIsDefinition( ins.eOpcode );
		switch ( ins.eOpcode ) {
		case SIR_LANG_OP_PHI: {
			const uint32_t iIncoming = frame.iPhiPredecessor == ins.nAuxiliary ? ins.iFirstSource : ins.iSecondSource;
			if ( !( frame.nInitialized & ( UINT64_C( 1 ) << iIncoming ) ) ) {
				output.eError = SIR_LANG_VM_UNINITIALIZED;
				return output;
			}
			value = frame.szRegisters[iIncoming];
			break;
		}
		case SIR_LANG_OP_CONST:
			value = { ins.nImmediate, ins.eType };
			break;
		case SIR_LANG_OP_PARAMETER:
			value = frame.szParameters[ins.iFirstSource];
			break;
		case SIR_LANG_OP_MOVE:
			value = first;
			break;
		case SIR_LANG_OP_ADD:
		case SIR_LANG_OP_SUBTRACT:
		case SIR_LANG_OP_MULTIPLY:
		case SIR_LANG_OP_DIVIDE:
			output.eError = sirLangIntegerOperation( ins, first, second, &value );
			if ( output.eError != SIR_LANG_VM_OK ) {
				return output;
			}
			break;
		case SIR_LANG_OP_EQUAL:
			value = { first.nBits == second.nBits ? 1U : 0U, SIR_LANG_VALUE_BOOL };
			break;
		case SIR_LANG_OP_LESS:
		case SIR_LANG_OP_LESS_EQUAL: {
			bool bCompare = false;
			if ( sirLangIsSignedInteger( first.eType ) ) {
				const uint32_t nWidth = sirLangIntegerWidth( first.eType );
				const int64_t nA = sirLangReadSigned( first.nBits, nWidth );
				const int64_t nB = sirLangReadSigned( second.nBits, nWidth );
				bCompare = ins.eOpcode == SIR_LANG_OP_LESS ? nA < nB : nA <= nB;
			} else {
				bCompare = ins.eOpcode == SIR_LANG_OP_LESS ? first.nBits < second.nBits : first.nBits <= second.nBits;
			}
			value = { bCompare ? 1U : 0U, SIR_LANG_VALUE_BOOL };
			break;
		}
		case SIR_LANG_OP_NOT:
			value = { first.nBits == 0 ? 1U : 0U, SIR_LANG_VALUE_BOOL };
			break;
		case SIR_LANG_OP_AND:
			value = { first.nBits != 0 && second.nBits != 0 ? 1U : 0U, SIR_LANG_VALUE_BOOL };
			break;
		case SIR_LANG_OP_OR:
			value = { first.nBits != 0 || second.nBits != 0 ? 1U : 0U, SIR_LANG_VALUE_BOOL };
			break;
		case SIR_LANG_OP_JUMP:
			frame.iInstruction = ins.nAuxiliary;
			break;
		case SIR_LANG_OP_JUMP_FALSE:
			if ( first.nBits == 0 ) {
				frame.iInstruction = ins.nAuxiliary;
			}
			break;
		case SIR_LANG_OP_CALL: {
			if ( nFrames >= SIR_LANG_VM_MAX_FRAMES ) {
				output.eError = SIR_LANG_VM_STACK_OVERFLOW;
				return output;
			}
			sirLangVMFrame_t& child = szFrames[nFrames];
			memset( &child, 0, sizeof( child ) );
			child.iFunction = ins.iFirstSource;
			child.iInstruction = pProgram->pFunctions[child.iFunction].iFirstInstruction;
			child.iReturnRegister = ins.iDestination;
			for ( uint32_t i = 0; i < ins.iSecondSource; ++i ) {
				child.szParameters[i] = frame.szRegisters[pProgram->pCallArguments[ins.nAuxiliary + i]];
			}
			++nFrames;
			bWritesDestination = false;
			break;
		}
		case SIR_LANG_OP_CALL_NATIVE: {
			if ( invocation.pInvokeNative == nullptr ) {
				output.eError = SIR_LANG_VM_INVALID_CAPABILITY;
				return output;
			}
			const sirLangCIRCapability_t& capability = pProgram->pCapabilities[ins.iFirstSource];
			if ( capability.nOwnerLane != 0 && capability.nOwnerLane != invocation.nOwnerLane ) {
				output.eError = SIR_LANG_VM_INVALID_CAPABILITY;
				return output;
			}
			sirLangVMValue_t szArguments[SIR_LANG_VM_MAX_PARAMETERS] = {};
			for ( uint32_t i = 0; i < ins.iSecondSource; ++i ) {
				szArguments[i] = frame.szRegisters[pProgram->pCallArguments[ins.nAuxiliary + i]];
			}
			const sirLangVMCall_t call = { capability.nFunctionId, capability.nSignatureId, capability.nBindingRevision,
			                               capability.nOwnerLane,  capability.nEffects,     szArguments,
			                               ins.iSecondSource };
			output.eError = invocation.pInvokeNative( call, &value, invocation.pUser );
			if ( output.eError != SIR_LANG_VM_OK ) {
				return output;
			}
			if ( value.eType != capability.eReturnType || !sirLangValidRuntimeValue( value ) ) {
				output.eError = SIR_LANG_VM_INVALID_TYPE;
				return output;
			}
			break;
		}
		case SIR_LANG_OP_RETURN: {
			const sirLangVMValue_t result = function.eReturnType == SIR_LANG_VALUE_VOID ? sirLangVMValue_t{} : first;
			const uint32_t iDestination = frame.iReturnRegister;
			--nFrames;
			if ( nFrames == 0 ) {
				output.result = result;
				output.eError = SIR_LANG_VM_OK;
				return output;
			}
			sirLangVMFrame_t& parent = szFrames[nFrames - 1];
			parent.szRegisters[iDestination] = result;
			parent.nInitialized |= UINT64_C( 1 ) << iDestination;
			bWritesDestination = false;
			break;
		}
		default:
			output.eError = SIR_LANG_VM_INVALID_OPCODE;
			return output;
		}
		if ( bWritesDestination ) {
			frame.szRegisters[ins.iDestination] = value;
			frame.nInitialized |= UINT64_C( 1 ) << ins.iDestination;
		}
	}
	output.eError = SIR_LANG_VM_FUEL_EXHAUSTED;
	return output;
}
