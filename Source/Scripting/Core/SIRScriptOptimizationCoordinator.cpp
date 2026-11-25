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

#include <Scripting/Core/SIRScriptOptimizationCoordinator.h>

SIRScriptOptimizationCoordinator::SIRScriptOptimizationCoordinator( void ) noexcept
    : m_Transport{},
      m_pSource( nullptr ),
      m_pManager( nullptr ),
      m_OwnerThread( SIRThread::currentThreadId() ),
      m_nNextJobId( 0 ),
      m_nNextSequence( 0 ),
      m_szJobs{},
      m_bConfigured( false ),
      m_bRunning( false ),
      m_bInCallback( false )
{
}

SIRScriptOptimizationCoordinator::~SIRScriptOptimizationCoordinator( void ) noexcept
{
	static_cast<void>( shutdown() );
}

uint32_t SIRScriptOptimizationCoordinator::nextGeneration( uint32_t nGeneration ) noexcept
{
	++nGeneration;
	return nGeneration == 0 ? 1 : nGeneration;
}

bool SIRScriptOptimizationCoordinator::isOwnerThread( void ) const noexcept
{
	return SIRThread::isCurrentThread( m_OwnerThread );
}

int32_t SIRScriptOptimizationCoordinator::resolve( sirScriptOptimizationJobHandle_t handle ) const noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > MAX_JOBS ) {
		return -1;
	}
	const sirScriptOptimizationJobSlot_t& slot = m_szJobs[handle.nSlot - 1];
	return slot.bOccupied && slot.nGeneration == handle.nGeneration ? static_cast<int32_t>( handle.nSlot - 1 ) : -1;
}

int32_t SIRScriptOptimizationCoordinator::findByJobId( uint64_t nJobId ) const noexcept
{
	for ( uint32_t i = 0; i < MAX_JOBS; ++i ) {
		if ( m_szJobs[i].bOccupied && m_szJobs[i].info.nJobId == nJobId && m_szJobs[i].info.bAwaitingReply ) {
			return static_cast<int32_t>( i );
		}
	}
	return -1;
}

SIRScriptResult SIRScriptOptimizationCoordinator::configure( const sirScriptOptimizationTransportAPI_t& transport,
                                                             SIRScriptCachedProgramSource *pSource,
                                                             SIRScriptCodeManager *pManager ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( m_bConfigured ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	if ( transport.nABIRevision != SIR_SCRIPT_OPT_TRANSPORT_ABI_REVISION || transport.pStart == nullptr || transport.pStop == nullptr ||
	     transport.pSend == nullptr || transport.pReceive == nullptr || pSource == nullptr || pManager == nullptr ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	m_Transport = transport;
	m_pSource = pSource;
	m_pManager = pManager;
	m_bConfigured = true;
	return SIRScriptResult::SIR_SCRIPT_OK;
}

SIRScriptResult SIRScriptOptimizationCoordinator::start( void ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured || m_bRunning ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	m_bInCallback = true;
	const SIRScriptResult eResult = m_Transport.pStart( m_Transport.pUser );
	m_bInCallback = false;
	if ( eResult == SIRScriptResult::SIR_SCRIPT_OK ) {
		m_bRunning = true;
	}
	return eResult;
}

SIRScriptResult
SIRScriptOptimizationCoordinator::submit( const sirScriptOptimizationRequest_t& request, bool bActivate, sirScriptOptimizationJobHandle_t *pHandle ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured || !m_bRunning ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	if ( pHandle == nullptr || request.nJobId != 0 || m_nNextJobId == UINT64_MAX || m_nNextSequence == UINT64_MAX ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	int32_t iFree = -1;
	for ( uint32_t i = 0; i < MAX_JOBS; ++i ) {
		if ( !m_szJobs[i].bOccupied ) {
			iFree = static_cast<int32_t>( i );
			break;
		}
	}
	if ( iFree < 0 ) {
		return SIRScriptResult::SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	sirScriptOptimizationRequest_t prepared = request;
	prepared.nJobId = m_nNextJobId + 1;
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES];
	memset( szWire, 0, sizeof( szWire ) );
	const SIRScriptResult eValid = SIRScriptOptimizationProtocol::encodeRequest( prepared, szWire );
	if ( eValid != SIRScriptResult::SIR_SCRIPT_OK ) {
		return eValid;
	}
	m_nNextJobId = prepared.nJobId;
	sirScriptOptimizationJobSlot_t& slot = m_szJobs[iFree];
	slot.nGeneration = nextGeneration( slot.nGeneration );
	slot.nSequence = ++m_nNextSequence;
	slot.request = prepared;
	slot.info = { prepared.nJobId, SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_QUEUED, SIRScriptResult::SIR_SCRIPT_BUSY, {}, {}, false };
	slot.bOccupied = true;
	slot.bActivate = bActivate;
	*pHandle = { static_cast<uint32_t>( iFree ) + 1, slot.nGeneration };
	return SIRScriptResult::SIR_SCRIPT_OK;
}

SIRScriptResult SIRScriptOptimizationCoordinator::cancel( sirScriptOptimizationJobHandle_t handle ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptOptimizationJobInfo_t& info = m_szJobs[iSlot].info;
	if ( info.eState != SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_QUEUED && info.eState != SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_IN_FLIGHT ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	// A dispatched cancellation is a tombstone until its late reply is drained
	// or the worker terminates. Never recycle that slot/id prematurely.
	info.eState = SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_CANCELED;
	info.eResult = SIRScriptResult::SIR_SCRIPT_BUSY;
	return SIRScriptResult::SIR_SCRIPT_OK;
}

SIRScriptResult SIRScriptOptimizationCoordinator::query( sirScriptOptimizationJobHandle_t handle, sirScriptOptimizationJobInfo_t *pInfo ) const noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( pInfo == nullptr ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_HANDLE;
	}
	*pInfo = m_szJobs[iSlot].info;
	return SIRScriptResult::SIR_SCRIPT_OK;
}

SIRScriptResult SIRScriptOptimizationCoordinator::release( sirScriptOptimizationJobHandle_t handle ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptOptimizationJobSlot_t& slot = m_szJobs[iSlot];
	if ( slot.info.bAwaitingReply || slot.info.eState == SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_QUEUED ||
	     slot.info.eState == SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_IN_FLIGHT ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	// Successful artifact ownership was transferred into source/code manager.
	// The caller owns their published code handle, not the job record.
	slot.request = {};
	slot.info = {};
	slot.bOccupied = false;
	return SIRScriptResult::SIR_SCRIPT_OK;
}

void SIRScriptOptimizationCoordinator::failJob( sirScriptOptimizationJobSlot_t& slot, SIRScriptResult eResult ) noexcept
{
	slot.info.eResult = eResult;
	slot.info.eState = SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_FAILED;
	slot.info.bAwaitingReply = false;
}

void SIRScriptOptimizationCoordinator::failWorker( void ) noexcept
{
	if ( m_bRunning ) {
		m_bInCallback = true;
		m_Transport.pStop( m_Transport.pUser );
		m_bInCallback = false;
		m_bRunning = false;
	}
	for ( uint32_t i = 0; i < MAX_JOBS; ++i ) {
		sirScriptOptimizationJobSlot_t& slot = m_szJobs[i];
		if ( !slot.bOccupied ) {
			continue;
		}
		if ( slot.info.eState == SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_IN_FLIGHT ) {
			failJob( slot, SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE );
		} else if ( slot.info.eState == SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_CANCELED ) {
			slot.info.bAwaitingReply = false;
		}
	}
}

SIRScriptResult SIRScriptOptimizationCoordinator::workerFailed( void ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	failWorker();
	return SIRScriptResult::SIR_SCRIPT_OK;
}

// Explicit retries must happen after the old transport is stopped. Its
// previous receive queue is discarded on stop, and wire IDs never repeat.
SIRScriptResult SIRScriptOptimizationCoordinator::retryFailed( sirScriptOptimizationJobHandle_t handle ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured || m_bRunning ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptOptimizationJobSlot_t& slot = m_szJobs[iSlot];
	if ( slot.info.eState != SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_FAILED || slot.info.eResult != SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE ||
	     slot.info.bAwaitingReply ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	if ( m_nNextJobId == UINT64_MAX || m_nNextSequence == UINT64_MAX ) {
		return SIRScriptResult::SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	sirScriptOptimizationRequest_t prepared = slot.request;
	prepared.nJobId = m_nNextJobId + 1;
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES];
	memset( szWire, 0, sizeof( szWire ) );
	const SIRScriptResult eValid = SIRScriptOptimizationProtocol::encodeRequest( prepared, szWire );
	if ( eValid != SIRScriptResult::SIR_SCRIPT_OK ) {
		return eValid;
	}
	m_nNextJobId = prepared.nJobId;
	slot.request = prepared;
	slot.nSequence = ++m_nNextSequence;
	slot.info = { prepared.nJobId, SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_QUEUED, SIRScriptResult::SIR_SCRIPT_BUSY, {}, {}, false };
	return SIRScriptResult::SIR_SCRIPT_OK;
}

void SIRScriptOptimizationCoordinator::processReply( const sirScriptOptimizationReply_t& reply, sirScriptOptimizationPumpStats_t *pStats ) noexcept
{
	const int32_t iSlot = findByJobId( reply.nJobId );
	if ( iSlot < 0 ) {
		failWorker();
		return;
	}
	sirScriptOptimizationJobSlot_t& slot = m_szJobs[iSlot];
	slot.info.bAwaitingReply = false;
	if ( slot.info.eState == SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_CANCELED ) {
		++pStats->nIgnoredCanceled;
		return;
	}
	const SIRScriptResult eValidated = SIRScriptOptimizationProtocol::validateCompletion( slot.request, reply );
	if ( eValidated != SIRScriptResult::SIR_SCRIPT_OK ) {
		failJob( slot, eValidated );
		++pStats->nFailed;
		return;
	}
	// Register first, acquire from verified disk through the code manager,
	// and roll back every partially acquired owner if staging/activation fails.
	sirScriptCachedEntryHandle_t entry{};
	m_bInCallback = true;
	SIRScriptResult eResult = m_pSource->registerArtifact( reply.cached, &entry );
	if ( eResult != SIRScriptResult::SIR_SCRIPT_OK ) {
		m_bInCallback = false;
		failJob( slot, eResult );
		++pStats->nFailed;
		return;
	}
	sirScriptCodeHandle_t code{};
	eResult = m_pManager->stage( reply.cached.artifact, &code );
	if ( eResult == SIRScriptResult::SIR_SCRIPT_OK && slot.bActivate ) {
		eResult = m_pManager->activate( code );
	}
	if ( eResult != SIRScriptResult::SIR_SCRIPT_OK ) {
		if ( code.nSlot != 0 ) {
			static_cast<void>( m_pManager->discard( code ) );
		}
		static_cast<void>( m_pSource->unregisterArtifact( entry ) );
		m_bInCallback = false;
		failJob( slot, eResult );
		++pStats->nFailed;
		return;
	}
	m_bInCallback = false;
	slot.info.code = code;
	slot.info.entry = entry;
	slot.info.eState = SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_COMPLETED;
	slot.info.eResult = SIRScriptResult::SIR_SCRIPT_OK;
	++pStats->nCompleted;
}

SIRScriptResult SIRScriptOptimizationCoordinator::pump( uint32_t nSendBudget, uint32_t nReceiveBudget, sirScriptOptimizationPumpStats_t *pStats ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured || !m_bRunning ) {
		return SIRScriptResult::SIR_SCRIPT_WRONG_STATE;
	}
	if ( pStats == nullptr ) {
		return SIRScriptResult::SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pStats = {};
	if ( nSendBudget > MAX_JOBS ) {
		nSendBudget = MAX_JOBS;
	}
	if ( nReceiveBudget > MAX_JOBS ) {
		nReceiveBudget = MAX_JOBS;
	}
	// Snapshot bounds new submissions to a later pump, even if slots recycle.
	const uint64_t nHighWater = m_nNextSequence;
	for ( uint32_t iSend = 0; iSend < nSendBudget; ++iSend ) {
		int32_t iNext = -1;
		uint64_t nFirstSequence = UINT64_MAX;
		for ( uint32_t i = 0; i < MAX_JOBS; ++i ) {
			const sirScriptOptimizationJobSlot_t& slot = m_szJobs[i];
			if ( slot.bOccupied && slot.info.eState == SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_QUEUED && slot.nSequence <= nHighWater &&
			     slot.nSequence < nFirstSequence ) {
				iNext = static_cast<int32_t>( i );
				nFirstSequence = slot.nSequence;
			}
		}
		if ( iNext < 0 ) {
			break;
		}
		sirScriptOptimizationJobSlot_t& slot = m_szJobs[iNext];
		uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES];
		memset( szWire, 0, sizeof( szWire ) );
		if ( SIRScriptOptimizationProtocol::encodeRequest( slot.request, szWire ) != SIRScriptResult::SIR_SCRIPT_OK ) {
			failJob( slot, SIRScriptResult::SIR_SCRIPT_TYPE_MISMATCH );
			++pStats->nFailed;
			continue;
		}
		m_bInCallback = true;
		const SIRScriptResult eSend = m_Transport.pSend( m_Transport.pUser, szWire, sizeof( szWire ) );
		m_bInCallback = false;
		if ( eSend == SIRScriptResult::SIR_SCRIPT_BUSY ) {
			break;
		}
		if ( eSend != SIRScriptResult::SIR_SCRIPT_OK ) {
			failWorker();
			return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
		}
		slot.info.eState = SIRScriptOptimizationJobState::SIR_SCRIPT_JOB_IN_FLIGHT;
		slot.info.bAwaitingReply = true;
		++pStats->nSent;
	}
	for ( uint32_t iReply = 0; iReply < nReceiveBudget; ++iReply ) {
		uint8_t szWire[SIRScriptOptimizationProtocol::REPLY_BYTES];
		memset( szWire, 0, sizeof( szWire ) );
		uint32_t nBytes = 0;
		m_bInCallback = true;
		const SIRScriptResult eReceive = m_Transport.pReceive( m_Transport.pUser, szWire, sizeof( szWire ), &nBytes );
		m_bInCallback = false;
		if ( eReceive == SIRScriptResult::SIR_SCRIPT_BUSY ) {
			break;
		}
		if ( eReceive != SIRScriptResult::SIR_SCRIPT_OK || nBytes != sizeof( szWire ) ) {
			failWorker();
			return SIRScriptResult::SIR_SCRIPT_BACKEND_FAILURE;
		}
		sirScriptOptimizationReply_t reply{};
		if ( SIRScriptOptimizationProtocol::decodeReply( szWire, nBytes, &reply ) != SIRScriptResult::SIR_SCRIPT_OK ) {
			failWorker();
			return SIRScriptResult::SIR_SCRIPT_TYPE_MISMATCH;
		}
		++pStats->nReceived;
		processReply( reply, pStats );
		if ( !m_bRunning ) {
			return SIRScriptResult::SIR_SCRIPT_TYPE_MISMATCH;
		}
	}
	return SIRScriptResult::SIR_SCRIPT_OK;
}

SIRScriptResult SIRScriptOptimizationCoordinator::shutdown( void ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIRScriptResult::SIR_SCRIPT_BUSY;
	}
	failWorker();
	// The caller retains completed code handles and explicitly owns the code
	// manager / cache source. Shutdown only drops coordinator job bookkeeping.
	for ( uint32_t i = 0; i < MAX_JOBS; ++i ) {
		m_szJobs[i].bOccupied = false;
		m_szJobs[i].info = {};
		m_szJobs[i].request = {};
	}
	return SIRScriptResult::SIR_SCRIPT_OK;
}
