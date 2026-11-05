#include <Core/SIRApplication.h>
#include <benchmark/benchmark.h>

static void SIRBenchmarkApplicationTick( benchmark::State& benchmarkState )
{
	SIRApplication application;
	if ( !application.initialize() ) {
		benchmarkState.SkipWithError( "SIRApplication initialization failed" );
		return;
	}

	for ( auto _ : benchmarkState ) {
		( void )_;
		benchmark::DoNotOptimize( application.tick() );
	}

	benchmark::DoNotOptimize( application.getTickCount() );
}

static void SIRBenchmarkApplicationLifecycle( benchmark::State& benchmarkState )
{
	for ( auto _ : benchmarkState ) {
		( void )_;
		SIRApplication application;
		benchmark::DoNotOptimize( application.initialize() );
		application.shutdown();
		benchmark::ClobberMemory();
	}
}

BENCHMARK( SIRBenchmarkApplicationTick );
BENCHMARK( SIRBenchmarkApplicationLifecycle );
