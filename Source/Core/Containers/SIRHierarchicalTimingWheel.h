/* SIREngine - MPL-2.0. Four-level 256-bucket timer wheel with generational cancellation. */
#ifndef SIR_HIERARCHICAL_TIMING_WHEEL_H
#define SIR_HIERARCHICAL_TIMING_WHEEL_H
#include <Core/Containers/SIRVector.h>
#include <stdint.h>

class SIRHierarchicalTimingWheel
{
public:
	struct Handle
	{
		uint32_t nIndex;
		uint32_t nGeneration;
	};

private:
	struct Timer
	{
		uint64_t nDeadline = 0;
		uint64_t nPayload = 0;
		uint32_t nNext = UINT32_MAX;
		uint32_t nGeneration = 1;
		bool bActive = false;
	};
	SIRVector<Timer> m_timers;
	uint32_t m_heads[4][256]{};
	uint32_t m_nFree = UINT32_MAX;
	uint64_t m_nCurrent = 0;
	uint32_t m_nActive = 0;
	uint32_t level( uint64_t nDeadline ) const noexcept
	{
		const uint64_t nDelta = nDeadline - m_nCurrent;
		if ( nDelta < 256 ) {
			return 0;
		}
		if ( nDelta < ( 1ull << 16 ) ) {
			return 1;
		}
		if ( nDelta < ( 1ull << 24 ) ) {
			return 2;
		}
		return 3;
	}
	void link( uint32_t iTimer ) noexcept
	{
		const uint32_t iLevel = level( m_timers[iTimer].nDeadline );
		const uint32_t iBucket = ( uint32_t )( ( m_timers[iTimer].nDeadline >> ( iLevel * 8 ) ) & 255 );
		m_timers[iTimer].nNext = m_heads[iLevel][iBucket];
		m_heads[iLevel][iBucket] = iTimer;
	}
	void cascade( uint32_t iLevel ) noexcept
	{
		const uint32_t iBucket = ( uint32_t )( ( m_nCurrent >> ( iLevel * 8 ) ) & 255 );
		uint32_t iTimer = m_heads[iLevel][iBucket];
		m_heads[iLevel][iBucket] = UINT32_MAX;
		while ( iTimer != UINT32_MAX ) {
			uint32_t iNext = m_timers[iTimer].nNext;
			if ( m_timers[iTimer].bActive ) {
				link( iTimer );
			} else {
				retire( iTimer );
			}
			iTimer = iNext;
		}
	}
	void retire( uint32_t iTimer ) noexcept
	{
		m_timers[iTimer].bActive = false;
		m_timers[iTimer].nNext = m_nFree;
		m_nFree = iTimer;
		++m_timers[iTimer].nGeneration;
		if ( m_timers[iTimer].nGeneration == 0 ) {
			++m_timers[iTimer].nGeneration;
		}
	}

public:
	explicit SIRHierarchicalTimingWheel( const sirContainerAllocator_t *pAllocator ) : m_timers( pAllocator )
	{
		for ( uint32_t iLevel = 0; iLevel < 4; ++iLevel ) {
			for ( uint32_t iBucket = 0; iBucket < 256; ++iBucket ) {
				m_heads[iLevel][iBucket] = UINT32_MAX;
			}
		}
	}
	sirContainerResult_t trySchedule( uint64_t nDeadline, uint64_t nPayload, Handle *pHandle )
	{
		if ( pHandle == nullptr || nDeadline < m_nCurrent || nDeadline - m_nCurrent >= ( 1ull << 32 ) ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint32_t iTimer = m_nFree;
		if ( iTimer == UINT32_MAX ) {
			sirContainerResult_t eResult = m_timers.tryEmplaceBack();
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
			iTimer = m_timers.getSize() - 1;
		} else {
			m_nFree = m_timers[iTimer].nNext;
		}
		Timer& timer = m_timers[iTimer];
		timer.nDeadline = nDeadline;
		timer.nPayload = nPayload;
		timer.bActive = true;
		link( iTimer );
		++m_nActive;
		*pHandle = { iTimer, timer.nGeneration };
		return SIR_CONTAINER_OK;
	}
	bool cancel( Handle handle ) noexcept
	{
		if ( handle.nIndex >= m_timers.getSize() ) {
			return false;
		}
		Timer& timer = m_timers[handle.nIndex];
		if ( !timer.bActive || timer.nGeneration != handle.nGeneration ) {
			return false;
		}
		timer.bActive = false;
		--m_nActive;
		return true;
	}
	template <typename Visitor>
	void advanceTo( uint64_t nTarget, Visitor&& visitor )
	{
		if ( nTarget < m_nCurrent ) {
			return;
		}
		while ( m_nCurrent <= nTarget ) {
			if ( m_nCurrent != 0 && ( m_nCurrent & 255 ) == 0 ) {
				if ( ( m_nCurrent & 0xffffff ) == 0 ) {
					cascade( 3 );
				}
				if ( ( m_nCurrent & 0xffff ) == 0 ) {
					cascade( 2 );
				}
				cascade( 1 );
			}
			const uint32_t iBucket = ( uint32_t )( m_nCurrent & 255 );
			while ( m_heads[0][iBucket] != UINT32_MAX ) {
				const uint32_t iTimer = m_heads[0][iBucket];
				Timer& timer = m_timers[iTimer];
				m_heads[0][iBucket] = timer.nNext;
				if ( !timer.bActive ) {
					retire( iTimer );
				} else if ( timer.nDeadline <= m_nCurrent ) {
					// A callback may attempt to cancel its own handle or schedule a
					// new timer (which can relocate m_timers). Mark this one inactive
					// before invoking user code, then access it only by index.
					const uint64_t nPayload = timer.nPayload;
					timer.bActive = false;
					--m_nActive;
					try {
						visitor( nPayload );
					} catch ( ... ) {
						// Restore the unconsumed timer and preserve its handle.
						m_timers[iTimer].bActive = true;
						++m_nActive;
						link( iTimer );
						throw;
					}
					retire( iTimer );
				} else {
					link( iTimer );
				}
			}
			if ( m_nCurrent == nTarget || m_nCurrent == UINT64_MAX ) {
				break;
			}
			++m_nCurrent;
		}
		if ( m_nCurrent != UINT64_MAX ) {
			++m_nCurrent;
		}
	}
	uint32_t getActiveCount( void ) const noexcept
	{
		return m_nActive;
	}
	uint64_t getCurrentTick( void ) const noexcept
	{
		return m_nCurrent;
	}
};
#endif
