/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_CONSOLE_SYSTEM_H__
#define __SIR_CONSOLE_SYSTEM_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Console/SIRConsoleLogSink.h>
#include <Core/Console/SIRConsoleTypes.h>
#include <Core/Containers/SIRSpan.h>
#include <Core/Diagnostics/Logging/SIRLoggerSystem.h>
#include <Core/Strings/SIRStringPool.h>
#include <Core/Systems/SIREngineSystem.h>
#include <type_traits>

typedef struct sirConsoleSystemConfig_s
{
	SIRStringPool *pPool{};
	SIRLoggerSystem *pLogger{};
	uint32_t nCVarCapacity{ 256 };
	uint32_t nCommandCapacity{ 64 };
	uint32_t nCommandBytes{ 512 };
	uint32_t nOverrideCapacity{ 512 };
	uint32_t nLogCapacity{ 64 };
	bool bHeadless{};
} sirConsoleSystemConfig_t;

class SIRConsoleSystem final : public SIREngineSystem
{
public:
	explicit SIRConsoleSystem( const sirConsoleSystemConfig_t& config ) noexcept;
	~SIRConsoleSystem() noexcept override;

	SIRConsoleSystem( const SIRConsoleSystem& ) = delete;
	SIRConsoleSystem( SIRConsoleSystem&& ) = delete;
	SIRConsoleSystem& operator=( const SIRConsoleSystem& ) = delete;
	SIRConsoleSystem& operator=( SIRConsoleSystem&& ) = delete;

	template <typename TValue>
	[[nodiscard]] SIRConsoleResult registerCVar( SIRStringAtom name, const TValue& initialValue, uint32_t nFlags, sirCVarHandle_t& outHandle ) noexcept
	{
		static_assert( isSupportedScalar<TValue>(), "Unsupported CVar type" );
		return registerScalar( name, &initialValue, scalarType<TValue>(), nFlags, outHandle );
	}

	template <typename TValue>
	[[nodiscard]] SIRConsoleResult getCVar( sirCVarHandle_t handle, TValue& outValue ) const noexcept
	{
		static_assert( isSupportedScalar<TValue>(), "Unsupported CVar type" );
		return getScalar( handle, scalarType<TValue>(), &outValue );
	}

	template <typename TValue>
	[[nodiscard]] SIRConsoleResult setCVar( sirCVarHandle_t handle, const TValue& value, SIRCVarSource eSource, uint32_t nIssuerId ) noexcept
	{
		static_assert( isSupportedScalar<TValue>(), "Unsupported CVar type" );
		return setScalar( handle, &value, scalarType<TValue>(), eSource, nIssuerId );
	}

	[[nodiscard]] SIRConsoleResult getStringCVar( sirCVarHandle_t handle, sirStringView_t& outValue ) const noexcept;
	[[nodiscard]] SIRConsoleResult registerStringCVar( SIRStringAtom name, sirStringView_t initialValue, uint32_t nFlags, sirCVarHandle_t& outHandle ) noexcept;
	[[nodiscard]] SIRConsoleResult setStringCVar( sirCVarHandle_t handle, sirStringView_t value, SIRCVarSource eSource, uint32_t nIssuerId ) noexcept;
	[[nodiscard]] SIRConsoleResult unregisterCVar( sirCVarHandle_t handle ) noexcept;
	[[nodiscard]] SIRConsoleResult unregisterModule( uint32_t nModuleId ) noexcept;

	template <typename TOwner, SIRConsoleResult ( TOwner::*Method )( SIRSpan<const sirStringView_t> ) noexcept>
	[[nodiscard]] SIRConsoleResult registerCommand( SIRStringAtom name, TOwner& owner, sirCommandHandle_t& outHandle ) noexcept
	{
		return registerCommandBinding( name, &owner, &invokeCommand<TOwner, Method>, outHandle );
	}

	[[nodiscard]] SIRConsoleResult submitCommand( sirStringView_t text ) noexcept;
	[[nodiscard]] SIRConsoleResult executeCommand( sirStringView_t text ) noexcept;

	[[nodiscard]] SIRSystemResult initialize( void ) noexcept override;
	void update( float fDeltaSeconds ) noexcept override;
	[[nodiscard]] SIRSystemResult shutdown( void ) noexcept override;
	[[nodiscard]] bool isInitialized( void ) const noexcept;

private:
	enum class ScalarType : uint8_t
	{
		SIR_BOOL,
		SIR_I32,
		SIR_U32,
		SIR_F32,
		SIR_I64,
		SIR_U64,
		SIR_F64
	};
	using CommandInvoker = SIRConsoleResult ( * )( void *, SIRSpan<const sirStringView_t> ) noexcept;

	template <typename TValue>
	static constexpr bool isSupportedScalar( void ) noexcept
	{
		return std::is_same<TValue, bool>::value || std::is_same<TValue, int32_t>::value || std::is_same<TValue, uint32_t>::value || std::is_same<TValue, float>::value ||
		       std::is_same<TValue, int64_t>::value || std::is_same<TValue, uint64_t>::value || std::is_same<TValue, double>::value;
	}

	template <typename TValue>
	static constexpr ScalarType scalarType( void ) noexcept
	{
		return std::is_same<TValue, bool>::value       ? ScalarType::SIR_BOOL
		       : std::is_same<TValue, int32_t>::value  ? ScalarType::SIR_I32
		       : std::is_same<TValue, uint32_t>::value ? ScalarType::SIR_U32
		       : std::is_same<TValue, float>::value    ? ScalarType::SIR_F32
		       : std::is_same<TValue, int64_t>::value  ? ScalarType::SIR_I64
		       : std::is_same<TValue, uint64_t>::value ? ScalarType::SIR_U64
		                                               : ScalarType::SIR_F64;
	}

	template <typename TOwner, SIRConsoleResult ( TOwner::*Method )( SIRSpan<const sirStringView_t> ) noexcept>
	static SIRConsoleResult invokeCommand( void *pOwner, SIRSpan<const sirStringView_t> arguments ) noexcept
	{
		return ( static_cast<TOwner *>( pOwner )->*Method )( arguments );
	}

	[[nodiscard]] SIRConsoleResult registerScalar( SIRStringAtom name, const void *pValue, ScalarType eType, uint32_t nFlags, sirCVarHandle_t& outHandle ) noexcept;
	[[nodiscard]] SIRConsoleResult getScalar( sirCVarHandle_t handle, ScalarType eType, void *pValue ) const noexcept;
	[[nodiscard]] SIRConsoleResult setScalar( sirCVarHandle_t handle, const void *pValue, ScalarType eType, SIRCVarSource eSource, uint32_t nIssuerId ) noexcept;
	[[nodiscard]] SIRConsoleResult registerCommandBinding( SIRStringAtom name, void *pOwner, CommandInvoker pInvoker, sirCommandHandle_t& outHandle ) noexcept;
	[[nodiscard]] SIRConsoleResult findEntry( SIRStringAtom name, uint32_t& nSlot ) const noexcept;
	[[nodiscard]] SIRConsoleResult validateCVar( sirCVarHandle_t handle, uint32_t& nSlot ) const noexcept;
	[[nodiscard]] SIRConsoleResult stageValue( uint32_t nSlot, const void *pValue, ScalarType eType, SIRCVarSource eSource, uint32_t nIssuerId, bool bRemove ) noexcept;
	[[nodiscard]] SIRConsoleResult parseAndSet( SIRStringAtom name, sirStringView_t value, SIRCVarSource eSource, uint32_t nIssuerId ) noexcept;
	[[nodiscard]] SIRConsoleResult pumpCommands( uint32_t nBudget ) noexcept;
	void applyPending( uint32_t nSlot ) noexcept;
	[[nodiscard]] uint32_t sourcePriority( SIRCVarSource eSource ) const noexcept;

	sirConsoleSystemConfig_t m_Config;
	bool m_bInitialized{};
	bool m_bStopping{};
	uint32_t m_nCVarCapacity{};
	uint32_t m_nCommandCapacity{};
	uint32_t m_nCommandBytes{};
	uint32_t m_nOverrideCapacity{};
	uint64_t m_nRevision{};
	uint64_t m_nSerial{ 1 };
	uint64_t m_nNextRequest{ 1 };
	SIRStringAtom m_NameAtoms[256]{};
	uint32_t m_nGeneration[256]{};
	uint32_t m_nFlags[256]{};
	uint8_t m_nKind[256]{};
	uint8_t m_nScalarType[256]{};
	bool m_bActive[256]{};
	uint64_t m_NarrowValues[256]{};
	uint64_t m_WideValues[256]{};
	char m_StringValues[256][256]{};
	uint32_t m_nOwners[256]{};
	void *m_pOwners[256]{};
	CommandInvoker m_pCommandInvokers[256]{};
	uint32_t m_nOverrideSlots[512]{};
	uint32_t m_nOverrideIssuers[512]{};
	uint8_t m_nOverrideSources[512]{};
	uint8_t m_nOverrideTypes[512]{};
	uint64_t m_OverrideNarrow[512]{};
	uint64_t m_OverrideWide[512]{};
	bool m_bOverrideActive[512]{};
	char m_CommandText[64][512]{};
	uint32_t m_nCommandLengths[64]{};
	std::atomic<uint32_t> m_nCommandWrite{};
	std::atomic<uint32_t> m_nCommandRead{};
	SIRConsoleLogSink m_LogSink;
};

#endif // __SIR_CONSOLE_SYSTEM_H__
