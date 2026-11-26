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

#ifndef __SIR_LOGTYPES_H__
#define __SIR_LOGTYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#if defined(SIR_PRAGMA_ONCE_SUPPORTED) && SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class sirLogSeverity : uint8_t
{
	SIR_LOG_TRACE,
	SIR_LOG_DEBUG,
	SIR_LOG_INFO,
	SIR_LOG_WARNING,
	SIR_LOG_ERROR,
	SIR_LOG_FATAL,
	SIR_LOG_OFF
};

enum class sirLogResult : uint8_t
{
	SIR_LOG_OK,
	SIR_LOG_ACCEPTED,
	SIR_LOG_FILTERED,
	SIR_LOG_DROPPED_FULL,
	SIR_LOG_UNREGISTERED,
	SIR_LOG_STALE,
	SIR_LOG_CLOSING,
	SIR_LOG_INVALID,
	SIR_LOG_UNSUPPORTED,
	SIR_LOG_LIMIT,
	SIR_LOG_BUSY,
	SIR_LOG_WRONG_THREAD,
	SIR_LOG_REENTRANT,
	SIR_LOG_FORMAT_ERROR
};

enum class sirLogRecordFlag : uint8_t
{
	SIR_LOG_TRUNCATED = 1,
	SIR_LOG_INVALID_TEXT = 2
};

typedef struct sirLogConfig_s
{
	uint32_t nProducerLanes{ 4 };
	uint32_t nRingCapacity{ 32 };
	uint32_t nHighSeverityReserve{ 4 };
	uint32_t nMessageBytes{ 512 };
	uint32_t nCategoryCapacity{ 32 };
	uint32_t nSinkCapacity{ 4 };
} sirLogConfig_t;

typedef struct sirLogProducerHandle_s
{
	uint32_t iSlot{};
	uint64_t nGeneration{};
} sirLogProducerHandle_t;

typedef struct sirLogSinkHandle_s
{
	uint32_t iSlot{};
	uint64_t nGeneration{};
} sirLogSinkHandle_t;

typedef struct sirLogSite_s
{
	uint32_t nCategory{};
	sirLogSeverity eSeverity{};
	uint32_t nSourceId{};
	uint32_t nSourceLine{};
	const char *pszSource{};
} sirLogSite_t;

typedef struct sirLogInput_s
{
	sirLogSite_t site{};
	const char *pMessage{};
	size_t nMessageBytes{};
} sirLogInput_t;

typedef struct sirLogRecord_s
{
	static constexpr uint32_t s_nMaxMessageBytes = 512;
	static constexpr uint32_t s_nMaxSourceBytes = 95;

	uint64_t nTimestampNs{};
	uint64_t nThreadId{};
	uint64_t nProducerGeneration{};
	uint64_t nProducerSequence{};
	uint64_t nDeliverySequence{};
	uint32_t iProducer{};
	uint32_t nCategory{};
	uint32_t nSourceId{};
	uint32_t nSourceLine{};
	uint32_t nMessageBytes{};
	sirLogSeverity eSeverity{};
	uint8_t nFlags{};
	char szSource[s_nMaxSourceBytes + 1]{};
	char szMessage[s_nMaxMessageBytes + 1]{};
} sirLogRecord_t;

typedef struct sirLoggerSystemConfig_s
{
	uint32_t nQueueCapacity{ 256 };
	uint32_t nMaximumMessageBytes{ sirLogRecord_t::s_nMaxMessageBytes };
	const char *pszFilePath{ nullptr };
	bool bEnableFile{ false };
	bool bEnableConsole{ true };
} sirLoggerSystemConfig_t;

typedef struct sirLogStats_s
{
	uint64_t nAccepted{};
	uint64_t nFiltered{};
	uint64_t nDroppedFull{};
	uint64_t nUnregistered{};
	uint64_t nRejectedRegistration{};
	uint64_t nTruncated{};
	uint64_t nInvalidText{};
	uint64_t nFormatErrors{};
	uint64_t nReentrant{};
	uint64_t nSinkLost{};
	uint64_t nSinkErrors{};
} sirLogStats_t;

#endif // __SIR_LOGTYPES_H__
