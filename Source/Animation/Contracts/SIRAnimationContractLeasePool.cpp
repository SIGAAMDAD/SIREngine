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

#include "SIRAnimationContractLeasePool.h"
#include "SIRAnimationContractValidation.h"

SIRAnimationContractLeasePool::SIRAnimationContractLeasePool( void ) noexcept
    : m_pPoses( nullptr ), m_pReaders( nullptr ), m_nPoseCapacity( 0 ), m_nReaderCapacity( 0 ), m_nOutstandingReaders( 0 ),
      m_nCancelledWorldEpoch( 0 ), m_nCancellationGeneration( 0 ), m_bHasCancellation( 0 )
{
}

uint32_t SIRAnimationContractLeasePool::getOutstandingReaders( void ) const noexcept
{
	return m_nOutstandingReaders;
}

sirAnimationResult_t SIRAnimationContractLeasePool::initialize( sirAnimationContractPoseSlot_t *pPoses, uint32_t nPoseCapacity,
                                                                sirAnimationContractReaderSlot_t *pReaders,
                                                                uint32_t nReaderCapacity ) noexcept
{
	if ( m_pPoses ) {
		return SIR_ANIMATION_BUSY;
	}
	if ( !pPoses || !pReaders || !nPoseCapacity || !nReaderCapacity ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	if ( ( uintptr_t )pPoses % alignof( sirAnimationContractPoseSlot_t ) != 0 ||
	     ( uintptr_t )pReaders % alignof( sirAnimationContractReaderSlot_t ) != 0 ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	const uint64_t nPoseBytes = ( uint64_t )sizeof( sirAnimationContractPoseSlot_t ) * nPoseCapacity;
	const uint64_t nReaderBytes = ( uint64_t )sizeof( sirAnimationContractReaderSlot_t ) * nReaderCapacity;
	const uintptr_t nPoseStart = ( uintptr_t )pPoses;
	const uintptr_t nReaderStart = ( uintptr_t )pReaders;
	if ( nPoseBytes > UINTPTR_MAX - nPoseStart || nReaderBytes > UINTPTR_MAX - nReaderStart ) {
		return SIR_ANIMATION_OUT_OF_CAPACITY;
	}
	const uintptr_t nPoseEnd = nPoseStart + nPoseBytes;
	const uintptr_t nReaderEnd = nReaderStart + nReaderBytes;
	if ( nPoseStart < nReaderEnd && nReaderStart < nPoseEnd ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	m_pPoses = pPoses;
	m_pReaders = pReaders;
	m_nPoseCapacity = nPoseCapacity;
	m_nReaderCapacity = nReaderCapacity;
	m_nOutstandingReaders = 0;
	m_bHasCancellation = 0;
	m_nCancelledWorldEpoch = 0;
	m_nCancellationGeneration = 0;
	memset( m_pPoses, 0, sizeof( sirAnimationContractPoseSlot_t ) * nPoseCapacity );
	memset( m_pReaders, 0, sizeof( sirAnimationContractReaderSlot_t ) * nReaderCapacity );
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractLeasePool::registerPose( sirAnimationPoseHandle_t pose, const sirAnimationInstance_t& instance,
                                                                  const sirAnimationTick_t& tick ) noexcept
{
	if ( !m_pPoses || pose.nSlot >= m_nPoseCapacity || pose.nGeneration == 0 ) {
		return SIR_ANIMATION_INVALID_HANDLE;
	}
	if ( SIRAnimationContractValidation::validateInstance( instance ) != SIR_ANIMATION_SUCCESS ) {
		return SIR_ANIMATION_STALE_ENTITY;
	}
	sirAnimationContractPoseSlot_t& slot = m_pPoses[pose.nSlot];
	if ( slot.nActiveReaders ) {
		return SIR_ANIMATION_BUSY;
	}
	if ( pose.nGeneration <= slot.nGeneration ) {
		return SIR_ANIMATION_STALE_POSE;
	}
	if ( m_bHasCancellation &&
	     ( tick.nWorldEpoch < m_nCancelledWorldEpoch ||
	       ( tick.nWorldEpoch == m_nCancelledWorldEpoch && tick.nCancellationGeneration <= m_nCancellationGeneration ) ) ) {
		return SIR_ANIMATION_CANCELLED;
	}
	slot.instance = instance;
	slot.tick = tick;
	slot.nGeneration = pose.nGeneration;
	slot.nActiveReaders = 0;
	slot.bActive = 1;
	slot.bCancelled = 0;
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractLeasePool::acquire( sirAnimationPoseHandle_t pose, sirAnimationPoseLease_t *pLease ) noexcept
{
	if ( !pLease ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	if ( !m_pPoses || pose.nSlot >= m_nPoseCapacity || pose.nGeneration == 0 ) {
		return SIR_ANIMATION_INVALID_HANDLE;
	}
	sirAnimationContractPoseSlot_t& slot = m_pPoses[pose.nSlot];
	if ( !slot.bActive || slot.nGeneration != pose.nGeneration || slot.bCancelled ) {
		return SIR_ANIMATION_STALE_POSE;
	}
	for ( uint32_t i = 0; i < m_nReaderCapacity; ++i ) {
		sirAnimationContractReaderSlot_t& reader = m_pReaders[i];
		if ( !reader.bActive && reader.nGeneration != UINT32_MAX ) {
			++reader.nGeneration;
			reader.pose = pose;
			reader.bActive = 1;
			++slot.nActiveReaders;
			++m_nOutstandingReaders;
			// nReaderEpoch encodes the pool's one-based ticket index; the
			// generation is the ABA guard when a ticket slot is reused.
			pLease->pose = pose;
			pLease->nReaderEpoch = ( uint64_t )i + 1;
			pLease->nLeaseGeneration = reader.nGeneration;
			pLease->nReserved = 0;
			return SIR_ANIMATION_SUCCESS;
		}
	}
	return SIR_ANIMATION_OUT_OF_CAPACITY;
}

sirAnimationResult_t SIRAnimationContractLeasePool::release( const sirAnimationPoseLease_t& lease ) noexcept
{
	if ( !m_pReaders || !lease.nReaderEpoch || lease.nReaderEpoch > m_nReaderCapacity || lease.nReserved != 0 ) {
		return SIR_ANIMATION_INVALID_HANDLE;
	}
	sirAnimationContractReaderSlot_t& reader = m_pReaders[lease.nReaderEpoch - 1];
	if ( !reader.bActive || reader.nGeneration != lease.nLeaseGeneration || reader.pose.nSlot != lease.pose.nSlot ||
	     reader.pose.nGeneration != lease.pose.nGeneration ) {
		return SIR_ANIMATION_INVALID_HANDLE;
	}
	if ( lease.pose.nSlot >= m_nPoseCapacity ) {
		return SIR_ANIMATION_INVALID_HANDLE;
	}
	sirAnimationContractPoseSlot_t& slot = m_pPoses[lease.pose.nSlot];
	if ( slot.nGeneration != lease.pose.nGeneration || !slot.nActiveReaders ) {
		return SIR_ANIMATION_STALE_POSE;
	}
	reader.bActive = 0;
	--slot.nActiveReaders;
	--m_nOutstandingReaders;
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractLeasePool::cancelEpoch( uint64_t nWorldEpoch, uint32_t nCancellationGeneration ) noexcept
{
	if ( !m_pPoses ) {
		return SIR_ANIMATION_INVALID_HANDLE;
	}
	if ( !m_bHasCancellation || nWorldEpoch > m_nCancelledWorldEpoch ||
	     ( nWorldEpoch == m_nCancelledWorldEpoch && nCancellationGeneration > m_nCancellationGeneration ) ) {
		m_bHasCancellation = 1;
		m_nCancelledWorldEpoch = nWorldEpoch;
		m_nCancellationGeneration = nCancellationGeneration;
	}
	for ( uint32_t i = 0; i < m_nPoseCapacity; ++i ) {
		sirAnimationContractPoseSlot_t& slot = m_pPoses[i];
		if ( slot.bActive && slot.tick.nWorldEpoch == nWorldEpoch && slot.tick.nCancellationGeneration <= nCancellationGeneration ) {
			slot.bCancelled = 1;
		}
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractLeasePool::retirePose( sirAnimationPoseHandle_t pose ) noexcept
{
	if ( !m_pPoses || pose.nSlot >= m_nPoseCapacity || pose.nGeneration == 0 ) {
		return SIR_ANIMATION_INVALID_HANDLE;
	}
	sirAnimationContractPoseSlot_t& slot = m_pPoses[pose.nSlot];
	if ( !slot.bActive || slot.nGeneration != pose.nGeneration ) {
		return SIR_ANIMATION_STALE_POSE;
	}
	if ( slot.nActiveReaders ) {
		return SIR_ANIMATION_BUSY;
	}
	slot.bActive = 0;
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractLeasePool::shutdown( void ) noexcept
{
	// Never release borrowed storage while a reader still retains its ticket.
	if ( m_nOutstandingReaders ) {
		return SIR_ANIMATION_BUSY;
	}
	m_pPoses = nullptr;
	m_pReaders = nullptr;
	m_nPoseCapacity = 0;
	m_nReaderCapacity = 0;
	m_bHasCancellation = 0;
	return SIR_ANIMATION_SUCCESS;
}
