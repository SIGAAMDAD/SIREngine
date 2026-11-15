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

static bool matches( const sirLifecycleTrace_t& trace, const uint32_t *pExpected, uint32_t nCount ) noexcept
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

static int testDependencyOrdering( void ) noexcept
{
	sirLifecycleTrace_t trace = {};
	SIRLifecycleTestSystem consumer( 3, &trace );
	SIRLifecycleTestSystem provider( 2, &trace );
	SIRLifecycleTestSystem foundation( 1, &trace );
	SIRApplication application;

	if ( !application.registerSystem( &consumer ) || !application.registerSystem( &provider ) ||
	     !application.registerSystem( &foundation ) ) {
		return 50;
	}
	if ( !application.addDependency( consumer, provider ) || !application.addDependency( provider, foundation ) ) {
		return 51;
	}
	if ( application.addDependency( consumer, provider ) || application.addDependency( foundation, foundation ) ) {
		return 51;
	}
	if ( !application.initialize() || application.getInitializationResult() != SIR_SYSTEM_OK || application.getFailedSystem() != nullptr ) {
		return 52;
	}
	if ( !application.tick( 0.01f ) || !application.fixedTick( 0.02f ) || application.shutdown() != SIR_SYSTEM_OK ) {
		return 52;
	}

	const uint32_t szExpected[] = { 11, 21, 31, 12, 22, 32, 13, 23, 33, 34, 24, 14 };
	return matches( trace, szExpected, 12 ) ? 0 : 53;
}

static int testIndependentDependencyOrdering( void ) noexcept
{
	sirLifecycleTrace_t trace = {};
	SIRLifecycleTestSystem consumer( 1, &trace );
	SIRLifecycleTestSystem independent( 2, &trace );
	SIRLifecycleTestSystem provider( 3, &trace );
	SIRApplication application;

	if ( !application.registerSystem( &consumer ) || !application.registerSystem( &independent ) ||
	     !application.registerSystem( &provider ) ) {
		return 60;
	}
	if ( !application.addDependency( consumer, provider ) ) {
		return 60;
	}
	if ( !application.initialize() || application.shutdown() != SIR_SYSTEM_OK ) {
		return 61;
	}

	const uint32_t szExpected[] = { 21, 31, 11, 14, 34, 24 };
	return matches( trace, szExpected, 6 ) ? 0 : 62;
}

static int testDependencyCycleAndUnknownSystem( void ) noexcept
{
	sirLifecycleTrace_t trace = {};
	SIRLifecycleTestSystem first( 1, &trace );
	SIRLifecycleTestSystem second( 2, &trace );
	SIRLifecycleTestSystem unregistered( 3, &trace );
	SIRApplication application;

	if ( !application.registerSystem( &first ) || !application.registerSystem( &second ) ) {
		return 70;
	}
	if ( application.addDependency( first, unregistered ) || application.addDependency( unregistered, first ) ) {
		return 70;
	}
	if ( !application.addDependency( first, second ) || !application.addDependency( second, first ) ) {
		return 70;
	}
	if ( application.initialize() || application.getState() != SIR_APP_STOPPED ||
	     application.getInitializationResult() != SIR_SYSTEM_ERROR ) {
		return 71;
	}
	if ( application.getFailedSystem() != nullptr || trace.nCount != 0 || application.shutdown() != SIR_SYSTEM_OK ) {
		return 71;
	}
	return 0;
}

static int testDependencyFailureRollback( void ) noexcept
{
	sirLifecycleTrace_t trace = {};
	SIRLifecycleTestSystem consumer( 3, &trace );
	SIRLifecycleTestSystem failing( 2, &trace );
	SIRLifecycleTestSystem provider( 1, &trace );
	failing.setInitializeResult( SIR_SYSTEM_BUSY );
	SIRApplication application;

	if ( !application.registerSystem( &consumer ) || !application.registerSystem( &failing ) || !application.registerSystem( &provider ) ) {
		return 80;
	}
	if ( !application.addDependency( consumer, failing ) || !application.addDependency( failing, provider ) || application.initialize() ) {
		return 80;
	}
	if ( application.getState() != SIR_APP_STOPPED || application.getInitializationResult() != SIR_SYSTEM_BUSY ||
	     application.getFailedSystem() != &failing ) {
		return 81;
	}

	const uint32_t szExpected[] = { 11, 21, 14 };
	return matches( trace, szExpected, 3 ) ? 0 : 82;
}

class SIRLifecycleReentrantSystem final : public SIREngineSystem
{
public:
	explicit SIRLifecycleReentrantSystem( SIRApplication& application ) noexcept;
	sirSystemResult_t initialize( void ) noexcept override;
	sirSystemResult_t shutdown( void ) noexcept override;

	sirSystemResult_t m_eRecursiveResult = SIR_SYSTEM_OK;
	uint32_t m_nShutdownCalls = 0;

private:
	SIRApplication *m_pApplication = nullptr;
};

SIRLifecycleReentrantSystem::SIRLifecycleReentrantSystem( SIRApplication& application ) noexcept : m_pApplication( &application )
{
}

sirSystemResult_t SIRLifecycleReentrantSystem::initialize( void ) noexcept
{
	return SIR_SYSTEM_OK;
}

sirSystemResult_t SIRLifecycleReentrantSystem::shutdown( void ) noexcept
{
	++m_nShutdownCalls;
	m_eRecursiveResult = m_pApplication->shutdown();
	return SIR_SYSTEM_OK;
}

static int testReentrantShutdown( void ) noexcept
{
	SIRApplication application;
	SIRLifecycleReentrantSystem system( application );
	if ( !application.registerSystem( &system ) || !application.initialize() || application.shutdown() != SIR_SYSTEM_OK ) {
		return 90;
	}
	if ( system.m_nShutdownCalls != 1 || system.m_eRecursiveResult != SIR_SYSTEM_BUSY || application.getState() != SIR_APP_STOPPED ) {
		return 91;
	}
	return 0;
}

class SIRLifecycleFrameCallbackSystem final : public SIREngineSystem
{
public:
	explicit SIRLifecycleFrameCallbackSystem( SIRApplication& application ) noexcept;
	sirSystemResult_t initialize( void ) noexcept override;
	void update( float fDeltaSeconds ) noexcept override;
	void fixedUpdate( float fDeltaSeconds ) noexcept override;
	sirSystemResult_t shutdown( void ) noexcept override;

	bool m_bNestedTickAccepted = false;
	bool m_bNestedFixedAccepted = false;
	sirSystemResult_t m_eShutdownDuringTick = SIR_SYSTEM_OK;
	sirSystemResult_t m_eShutdownDuringFixed = SIR_SYSTEM_OK;
	uint32_t m_nShutdownCalls = 0;

private:
	SIRApplication *m_pApplication = nullptr;
};

SIRLifecycleFrameCallbackSystem::SIRLifecycleFrameCallbackSystem( SIRApplication& application ) noexcept : m_pApplication( &application )
{
}

sirSystemResult_t SIRLifecycleFrameCallbackSystem::initialize( void ) noexcept
{
	return SIR_SYSTEM_OK;
}

void SIRLifecycleFrameCallbackSystem::update( float fDeltaSeconds ) noexcept
{
	m_bNestedTickAccepted = m_pApplication->tick( fDeltaSeconds );
	m_eShutdownDuringTick = m_pApplication->shutdown();
}

void SIRLifecycleFrameCallbackSystem::fixedUpdate( float fDeltaSeconds ) noexcept
{
	m_bNestedFixedAccepted = m_pApplication->fixedTick( fDeltaSeconds );
	m_eShutdownDuringFixed = m_pApplication->shutdown();
}

sirSystemResult_t SIRLifecycleFrameCallbackSystem::shutdown( void ) noexcept
{
	++m_nShutdownCalls;
	return SIR_SYSTEM_OK;
}

static int testFrameReentrancy( void ) noexcept
{
	SIRApplication application;
	SIRLifecycleFrameCallbackSystem system( application );
	if ( !application.registerSystem( &system ) || !application.initialize() || !application.tick( 0.01f ) ) {
		return 92;
	}
	if ( !application.fixedTick( 0.02f ) ) {
		return 92;
	}
	if ( system.m_bNestedTickAccepted || system.m_bNestedFixedAccepted || system.m_eShutdownDuringTick != SIR_SYSTEM_BUSY ) {
		return 93;
	}
	if ( system.m_eShutdownDuringFixed != SIR_SYSTEM_BUSY || application.getTickCount() != 1 || application.getState() != SIR_APP_READY ) {
		return 93;
	}
	return application.shutdown() == SIR_SYSTEM_OK && system.m_nShutdownCalls == 1 ? 0 : 94;
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
	const int iCapacity = testCapacity();
	if ( iCapacity != 0 ) {
		return iCapacity;
	}
	const int iDependencyOrder = testDependencyOrdering();
	if ( iDependencyOrder != 0 ) {
		return iDependencyOrder;
	}
	const int iIndependent = testIndependentDependencyOrdering();
	if ( iIndependent != 0 ) {
		return iIndependent;
	}
	const int iCycle = testDependencyCycleAndUnknownSystem();
	if ( iCycle != 0 ) {
		return iCycle;
	}
	const int iFailure = testDependencyFailureRollback();
	if ( iFailure != 0 ) {
		return iFailure;
	}
	const int iShutdown = testReentrantShutdown();
	if ( iShutdown != 0 ) {
		return iShutdown;
	}
	return testFrameReentrancy();
}
