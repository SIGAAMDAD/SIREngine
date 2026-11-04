#include <Core/SIRApplication.h>

typedef struct sirLifecycleTrace_s
{
	uint32_t szEvents[128];
	uint32_t nCount;
} sirLifecycleTrace_t;

class SIRLifecycleTestSystem final : public SIREngineSystem
{
public:
	SIRLifecycleTestSystem( void ) noexcept = default;
	SIRLifecycleTestSystem( uint32_t nId, sirLifecycleTrace_t *pTrace ) noexcept;
	~SIRLifecycleTestSystem( void ) noexcept override = default;

	void setInitializeResult( sirSystemResult_t eResult ) noexcept;
	void setShutdownResult( sirSystemResult_t eResult ) noexcept;

	sirSystemResult_t initialize( void ) noexcept override;
	void update( float fDeltaSeconds ) noexcept override;
	void fixedUpdate( float fFixedDeltaSeconds ) noexcept override;
	sirSystemResult_t shutdown( void ) noexcept override;

protected:
private:
	uint32_t m_nId = 0;
	sirLifecycleTrace_t *m_pTrace = nullptr;
	sirSystemResult_t m_eInitResult = SIR_SYSTEM_OK;
	sirSystemResult_t m_eShutdownResult = SIR_SYSTEM_OK;

	void record( uint32_t nPhase ) noexcept;
};

SIRLifecycleTestSystem::SIRLifecycleTestSystem( uint32_t nId, sirLifecycleTrace_t *pTrace ) noexcept : m_nId( nId ), m_pTrace( pTrace )
{
}

void SIRLifecycleTestSystem::setInitializeResult( sirSystemResult_t eResult ) noexcept
{
	m_eInitResult = eResult;
}

void SIRLifecycleTestSystem::setShutdownResult( sirSystemResult_t eResult ) noexcept
{
	m_eShutdownResult = eResult;
}

void SIRLifecycleTestSystem::record( uint32_t nPhase ) noexcept
{
	if ( m_pTrace == nullptr ) {
		return;
	}
	m_pTrace->szEvents[m_pTrace->nCount] = m_nId * 10 + nPhase;
	++m_pTrace->nCount;
}

sirSystemResult_t SIRLifecycleTestSystem::initialize( void ) noexcept
{
	record( 1 );
	return m_eInitResult;
}

void SIRLifecycleTestSystem::update( float fDeltaSeconds ) noexcept
{
	( void )fDeltaSeconds;
	record( 2 );
}

void SIRLifecycleTestSystem::fixedUpdate( float fFixedDeltaSeconds ) noexcept
{
	( void )fFixedDeltaSeconds;
	record( 3 );
}

sirSystemResult_t SIRLifecycleTestSystem::shutdown( void ) noexcept
{
	record( 4 );
	return m_eShutdownResult;
}

static bool matches( const sirLifecycleTrace_t &trace, const uint32_t *pExpected, uint32_t nCount ) noexcept
{
	if ( trace.nCount != nCount ) {
		return false;
	}
	for ( uint32_t iEvent = 0; iEvent < nCount; ++iEvent ) {
		if ( trace.szEvents[iEvent] != pExpected[iEvent] ) {
			return false;
		}
	}
	return true;
}

static int testSuccessfulLifecycle( void ) noexcept
{
	sirLifecycleTrace_t trace = {};
	SIRLifecycleTestSystem first( 1, &trace );
	SIRLifecycleTestSystem second( 2, &trace );
	SIRApplication application;
	if ( application.getState() != SIR_APP_UNINITIALIZED || application.tick() || application.fixedTick( 0.02f ) ) {
		return 1;
	}
	if ( application.registerSystem( nullptr ) || !application.registerSystem( &first ) || application.registerSystem( &first ) ) {
		return 2;
	}
	if ( !application.registerSystem( &second ) || application.getSystemCount() != 2 ) {
		return 2;
	}
	if ( !application.initialize() || application.getState() != SIR_APP_READY ) {
		return 3;
	}
	if ( application.initialize() || application.registerSystem( &first ) ) {
		return 3;
	}
	if ( !application.tick( 0.016f ) || !application.fixedTick( 0.02f ) || application.getTickCount() != 1 ) {
		return 4;
	}
	const uint32_t szExpectedRunning[] = { 11, 21, 12, 22, 13, 23 };
	if ( !matches( trace, szExpectedRunning, 6 ) ) {
		return 5;
	}
	if ( application.shutdown() != SIR_SYSTEM_OK || application.getState() != SIR_APP_STOPPED ) {
		return 6;
	}
	const uint32_t szExpectedStopped[] = { 11, 21, 12, 22, 13, 23, 24, 14 };
	if ( !matches( trace, szExpectedStopped, 8 ) || application.tick() || application.fixedTick( 0.02f ) ) {
		return 7;
	}
	if ( application.shutdown() != SIR_SYSTEM_OK || application.initialize() || application.registerSystem( &first ) ) {
		return 7;
	}
	return 0;
}

static int testInitializationRollback( void ) noexcept
{
	sirLifecycleTrace_t trace = {};
	SIRLifecycleTestSystem first( 1, &trace );
	SIRLifecycleTestSystem failing( 2, &trace );
	failing.setInitializeResult( SIR_SYSTEM_ERROR );
	SIRApplication application;
	if ( !application.registerSystem( &first ) || !application.registerSystem( &failing ) || application.initialize() ) {
		return 10;
	}
	const uint32_t szExpected[] = { 11, 21, 14 };
	if ( !matches( trace, szExpected, 3 ) || application.getState() != SIR_APP_STOPPED || application.tick() ) {
		return 11;
	}
	if ( application.shutdown() != SIR_SYSTEM_OK ) {
		return 11;
	}
	return 0;
}

static int testRetryableShutdown( void ) noexcept
{
	sirLifecycleTrace_t trace = {};
	SIRLifecycleTestSystem first( 1, &trace );
	SIRLifecycleTestSystem second( 2, &trace );
	second.setShutdownResult( SIR_SYSTEM_BUSY );
	SIRApplication application;
	if ( !application.registerSystem( &first ) || !application.registerSystem( &second ) || !application.initialize() ) {
		return 20;
	}
	if ( application.shutdown() != SIR_SYSTEM_BUSY || application.getState() != SIR_APP_STOPPING || application.tick() ) {
		return 21;
	}
	second.setShutdownResult( SIR_SYSTEM_OK );
	if ( application.shutdown() != SIR_SYSTEM_OK || application.getState() != SIR_APP_STOPPED ) {
		return 22;
	}
	const uint32_t szExpected[] = { 11, 21, 24, 24, 14 };
	if ( !matches( trace, szExpected, 5 ) ) {
		return 23;
	}
	return 0;
}

static int testRollbackRetry( void ) noexcept
{
	sirLifecycleTrace_t trace = {};
	SIRLifecycleTestSystem first( 1, &trace );
	SIRLifecycleTestSystem failing( 2, &trace );
	first.setShutdownResult( SIR_SYSTEM_BUSY );
	failing.setInitializeResult( SIR_SYSTEM_ERROR );
	SIRApplication application;
	if ( !application.registerSystem( &first ) || !application.registerSystem( &failing ) ) {
		return 30;
	}
	if ( application.initialize() || application.getState() != SIR_APP_STOPPING ) {
		return 30;
	}
	first.setShutdownResult( SIR_SYSTEM_OK );
	if ( application.shutdown() != SIR_SYSTEM_OK || application.getState() != SIR_APP_STOPPED ) {
		return 31;
	}
	const uint32_t szExpected[] = { 11, 21, 14, 14 };
	if ( !matches( trace, szExpected, 4 ) ) {
		return 32;
	}
	return 0;
}

static int testCapacity( void ) noexcept
{
	SIRLifecycleTestSystem szSystems[SIR_APPLICATION_MAX_SYSTEMS + 1];
	SIRApplication application;
	for ( uint32_t iSystem = 0; iSystem < SIR_APPLICATION_MAX_SYSTEMS; ++iSystem ) {
		if ( !application.registerSystem( &szSystems[iSystem] ) ) {
			return 40;
		}
	}
	if ( application.registerSystem( &szSystems[SIR_APPLICATION_MAX_SYSTEMS] ) ||
	     application.getSystemCount() != SIR_APPLICATION_MAX_SYSTEMS || !application.initialize() ) {
		return 41;
	}
	if ( application.shutdown() != SIR_SYSTEM_OK ) {
		return 41;
	}
	return 0;
}

int main( void )
{
	const int iSuccessful = testSuccessfulLifecycle();
	if ( iSuccessful != 0 ) {
		return iSuccessful;
	}
	const int iRollback = testInitializationRollback();
	if ( iRollback != 0 ) {
		return iRollback;
	}
	const int iRetry = testRetryableShutdown();
	if ( iRetry != 0 ) {
		return iRetry;
	}
	const int iRollbackRetry = testRollbackRetry();
	if ( iRollbackRetry != 0 ) {
		return iRollbackRetry;
	}
	return testCapacity();
}
