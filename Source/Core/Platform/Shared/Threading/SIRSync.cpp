#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>

SIRSpinLock::SIRSpinLock( void ) noexcept
{
}

void SIRSpinLock::lock( void ) noexcept
{
	uint32_t nSpins = 0;
	while ( m_Lock.exchange( true, std::memory_order_acquire ) ) {
		while ( m_Lock.load( std::memory_order_relaxed ) ) {
			SIRThread::cpuRelax();
			++nSpins;
			if ( nSpins == 1024 ) {
				SIRThread::yieldThread();
				nSpins = 0;
			}
		}
	}
}

bool SIRSpinLock::tryLock( void ) noexcept
{
	return !m_Lock.exchange( true, std::memory_order_acquire );
}

void SIRSpinLock::unlock( void ) noexcept
{
	m_Lock.store( false, std::memory_order_release );
}
