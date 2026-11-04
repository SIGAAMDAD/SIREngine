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
    : m_Transport{}, m_pSource( nullptr ), m_pManager( nullptr ), m_OwnerThread( std::this_thread::get_id() ), m_nNextJobId( 0 ),
      m_nNextSequence( 0 ), m_szJobs{}, m_bConfigured( false ), m_bRunning( false ), m_bInCallback( false )
{
}

SIRScriptOptimizationCoordinator::~SIRScriptOptimizationCoordinator( void ) noexcept
{
	( void )shutdown();
}

uint32_t SIRScriptOptimizationCoordinator::nextGeneration( uint32_t nGeneration ) noexcept
{
	++nGeneration;
	return nGeneration == 0 ? 1 : nGeneration;
}

bool SIRScriptOptimizationCoordinator::isOwnerThread( void ) const noexcept
{
	return std::this_thread::get_id() == m_OwnerThread;
}

int32_t SIRScriptOptimizationCoordinator::resolve( sirScriptOptimizationJobHandle_t handle ) const noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > MAX_JOBS ) {
		return -1;
	}
	const sirScriptOptimizationJobSlot_t &slot = m_szJobs[handle.nSlot - 1];
	return slot.bOccupied && slot.nGeneration == handle.nGeneration ? ( int32_t )( handle.nSlot - 1 ) : -1;
}

int32_t SIRScriptOptimizationCoordinator::findByJobId( uint64_t nJobId ) const noexcept
{
	for ( uint32_t i = 0; i < MAX_JOBS; ++i ) {
		if ( m_szJobs[i].bOccupied && m_szJobs[i].info.nJobId == nJobId && m_szJobs[i].info.bAwaitingReply ) {
			return ( int32_t )i;
		}
	}
	return -1;
}

sirScriptResult_t SIRScriptOptimizationCoordinator::configure( const sirScriptOptimizationTransportAPI_t &transport,
                                                               SIRScriptCachedProgramSource *pSource,
                                                               SIRScriptCodeManager *pManager ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_bConfigured ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( transport.nABIRevision != SIR_SCRIPT_OPT_TRANSPORT_ABI_REVISION || transport.pStart == nullptr || transport.pStop == nullptr ||
	     transport.pSend == nullptr || transport.pReceive == nullptr || pSource == nullptr || pManager == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	m_Transport = transport;
	m_pSource = pSource;
	m_pManager = pManager;
	m_bConfigured = true;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationCoordinator::start( void ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured || m_bRunning ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	m_bInCallback = true;
	const sirScriptResult_t eResult = m_Transport.pStart( m_Transport.pUser );
	m_bInCallback = false;
	if ( eResult == SIR_SCRIPT_OK ) {
		m_bRunning = true;
	}
	return eResult;
}

sirScriptResult_t SIRScriptOptimizationCoordinator::submit( const sirScriptOptimizationRequest_t &request, bool bActivate,
                                                            sirScriptOptimizationJobHandle_t *pHandle ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured || !m_bRunning ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pHandle == nullptr || request.nJobId != 0 || m_nNextJobId == UINT64_MAX || m_nNextSequence == UINT64_MAX ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	int32_t iFree = -1;
	for ( uint32_t i = 0; i < MAX_JOBS; ++i ) {
		if ( !m_szJobs[i].bOccupied ) {
			iFree = ( int32_t )i;
			break;
		}
	}
	if ( iFree < 0 ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	sirScriptOptimizationRequest_t prepared = request;
	prepared.nJobId = m_nNextJobId + 1;
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	const sirScriptResult_t eValid = SIRScriptOptimizationProtocol::encodeRequest( prepared, szWire );
	if ( eValid != SIR_SCRIPT_OK ) {
		return eValid;
	}
	m_nNextJobId = prepared.nJobId;
	sirScriptOptimizationJobSlot_t &slot = m_szJobs[iFree];
	slot.nGeneration = nextGeneration( slot.nGeneration );
	slot.nSequence = ++m_nNextSequence;
	slot.request = prepared;
	slot.info = { prepared.nJobId, SIR_SCRIPT_JOB_QUEUED, SIR_SCRIPT_BUSY, {}, {}, false };
	slot.bOccupied = true;
	slot.bActivate = bActivate;
	*pHandle = { ( uint32_t )iFree + 1, slot.nGeneration };
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationCoordinator::cancel( sirScriptOptimizationJobHandle_t handle ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptOptimizationJobInfo_t &info = m_szJobs[iSlot].info;
	if ( info.eState != SIR_SCRIPT_JOB_QUEUED && info.eState != SIR_SCRIPT_JOB_IN_FLIGHT ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	// A dispatched cancellation is a tombstone until its late reply is drained
	// or the worker terminates. Never recycle that slot/id prematurely.
	info.eState = SIR_SCRIPT_JOB_CANCELED;
	info.eResult = SIR_SCRIPT_BUSY;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationCoordinator::query( sirScriptOptimizationJobHandle_t handle,
                                                           sirScriptOptimizationJobInfo_t *pInfo ) const noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( pInfo == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	*pInfo = m_szJobs[iSlot].info;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationCoordinator::release( sirScriptOptimizationJobHandle_t handle ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIR_SCRIPT_BUSY;
	}
	const int32_t iSlot = resolve( handle );
	if ( iSlot < 0 ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptOptimizationJobSlot_t &slot = m_szJobs[iSlot];
	if ( slot.info.bAwaitingReply || slot.info.eState == SIR_SCRIPT_JOB_QUEUED || slot.info.eState == SIR_SCRIPT_JOB_IN_FLIGHT ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	// Successful artifact ownership was transferred into source/code manager.
	// The caller owns their published code handle, not the job record.
	slot.request = {};
	slot.info = {};
	slot.bOccupied = false;
	return SIR_SCRIPT_OK;
}

void SIRScriptOptimizationCoordinator::failJob( sirScriptOptimizationJobSlot_t &slot, sirScriptResult_t eResult ) noexcept
{
	slot.info.eResult = eResult;
	slot.info.eState = SIR_SCRIPT_JOB_FAILED;
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
		sirScriptOptimizationJobSlot_t &slot = m_szJobs[i];
		if ( !slot.bOccupied ) {
			continue;
		}
		if ( slot.info.eState == SIR_SCRIPT_JOB_IN_FLIGHT ) {
			failJob( slot, SIR_SCRIPT_BACKEND_FAILURE );
		} else if ( slot.info.eState == SIR_SCRIPT_JOB_CANCELED ) {
			slot.info.bAwaitingReply = false;
		}
	}
}

sirScriptResult_t SIRScriptOptimizationCoordinator::workerFailed( void ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	failWorker();
	return SIR_SCRIPT_OK;
}

void SIRScriptOptimizationCoordinator::processReply( const sirScriptOptimizationReply_t &reply,
                                                     sirScriptOptimizationPumpStats_t *pStats ) noexcept
{
	const int32_t iSlot = findByJobId( reply.nJobId );
	if ( iSlot < 0 ) {
		failWorker();
		return;
	}
	sirScriptOptimizationJobSlot_t &slot = m_szJobs[iSlot];
	slot.info.bAwaitingReply = false;
	if ( slot.info.eState == SIR_SCRIPT_JOB_CANCELED ) {
		++pStats->nIgnoredCanceled;
		return;
	}
	const sirScriptResult_t eValidated = SIRScriptOptimizationProtocol::validateCompletion( slot.request, reply );
	if ( eValidated != SIR_SCRIPT_OK ) {
		failJob( slot, eValidated );
		++pStats->nFailed;
		return;
	}
	// Register first, acquire from verified disk through the code manager,
	// and roll back every partially acquired owner if staging/activation fails.
	sirScriptCachedEntryHandle_t entry = {};
	m_bInCallback = true;
	sirScriptResult_t eResult = m_pSource->registerArtifact( reply.cached, &entry );
	if ( eResult != SIR_SCRIPT_OK ) {
		m_bInCallback = false;
		failJob( slot, eResult );
		++pStats->nFailed;
		return;
	}
	sirScriptCodeHandle_t code = {};
	eResult = m_pManager->stage( reply.cached.artifact, &code );
	if ( eResult == SIR_SCRIPT_OK && slot.bActivate ) {
		eResult = m_pManager->activate( code );
	}
	if ( eResult != SIR_SCRIPT_OK ) {
		if ( code.nSlot != 0 ) {
			( void )m_pManager->discard( code );
		}
		( void )m_pSource->unregisterArtifact( entry );
		m_bInCallback = false;
		failJob( slot, eResult );
		++pStats->nFailed;
		return;
	}
	m_bInCallback = false;
	slot.info.code = code;
	slot.info.entry = entry;
	slot.info.eState = SIR_SCRIPT_JOB_COMPLETED;
	slot.info.eResult = SIR_SCRIPT_OK;
	++pStats->nCompleted;
}

sirScriptResult_t SIRScriptOptimizationCoordinator::pump( uint32_t nSendBudget, uint32_t nReceiveBudget,
                                                          sirScriptOptimizationPumpStats_t *pStats ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( !m_bConfigured || !m_bRunning ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pStats == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
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
			const sirScriptOptimizationJobSlot_t &slot = m_szJobs[i];
			if ( slot.bOccupied && slot.info.eState == SIR_SCRIPT_JOB_QUEUED && slot.nSequence <= nHighWater &&
			     slot.nSequence < nFirstSequence ) {
				iNext = ( int32_t )i;
				nFirstSequence = slot.nSequence;
			}
		}
		if ( iNext < 0 ) {
			break;
		}
		sirScriptOptimizationJobSlot_t &slot = m_szJobs[iNext];
		uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
		if ( SIRScriptOptimizationProtocol::encodeRequest( slot.request, szWire ) != SIR_SCRIPT_OK ) {
			failJob( slot, SIR_SCRIPT_TYPE_MISMATCH );
			++pStats->nFailed;
			continue;
		}
		m_bInCallback = true;
		const sirScriptResult_t eSend = m_Transport.pSend( m_Transport.pUser, szWire, sizeof( szWire ) );
		m_bInCallback = false;
		if ( eSend == SIR_SCRIPT_BUSY ) {
			break;
		}
		if ( eSend != SIR_SCRIPT_OK ) {
			failWorker();
			return SIR_SCRIPT_BACKEND_FAILURE;
		}
		slot.info.eState = SIR_SCRIPT_JOB_IN_FLIGHT;
		slot.info.bAwaitingReply = true;
		++pStats->nSent;
	}
	for ( uint32_t iReply = 0; iReply < nReceiveBudget; ++iReply ) {
		uint8_t szWire[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
		uint32_t nBytes = 0;
		m_bInCallback = true;
		const sirScriptResult_t eReceive = m_Transport.pReceive( m_Transport.pUser, szWire, sizeof( szWire ), &nBytes );
		m_bInCallback = false;
		if ( eReceive == SIR_SCRIPT_BUSY ) {
			break;
		}
		if ( eReceive != SIR_SCRIPT_OK || nBytes != sizeof( szWire ) ) {
			failWorker();
			return SIR_SCRIPT_BACKEND_FAILURE;
		}
		sirScriptOptimizationReply_t reply = {};
		if ( SIRScriptOptimizationProtocol::decodeReply( szWire, nBytes, &reply ) != SIR_SCRIPT_OK ) {
			failWorker();
			return SIR_SCRIPT_TYPE_MISMATCH;
		}
		++pStats->nReceived;
		processReply( reply, pStats );
		if ( !m_bRunning ) {
			return SIR_SCRIPT_TYPE_MISMATCH;
		}
	}
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationCoordinator::shutdown( void ) noexcept
{
	if ( !isOwnerThread() || m_bInCallback ) {
		return SIR_SCRIPT_BUSY;
	}
	failWorker();
	// The caller retains completed code handles and explicitly owns the code
	// manager / cache source. Shutdown only drops coordinator job bookkeeping.
	for ( uint32_t i = 0; i < MAX_JOBS; ++i ) {
		m_szJobs[i].bOccupied = false;
		m_szJobs[i].info = {};
		m_szJobs[i].request = {};
	}
	return SIR_SCRIPT_OK;
}
