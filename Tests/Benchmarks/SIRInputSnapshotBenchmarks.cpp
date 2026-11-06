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

#include <Core/Platform/Input/SIRInputService.h>
#include <benchmark/benchmark.h>

class SIRInputSnapshotBenchmarkBackend final : public SIRInputBackend
{
public:
	sirInputBackendId_t id( void ) const noexcept override
	{
		return SIR_INPUT_TEST;
	}
	void sample( SIRInputService& service ) noexcept override
	{
		( void )service;
	}
	void resynchronize( SIRInputService& service ) noexcept override
	{
		( void )service;
	}
	bool vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )id;
		( void )nLeft;
		( void )nRight;
		return false;
	}
	void stop( SIRInputService& service ) noexcept override
	{
		( void )service;
	}
};

static void SIRBenchmarkInputSnapshotDirtyDevices( benchmark::State& state )
{
	SIRInputService input;
	SIRInputSnapshotBenchmarkBackend backend;
	sirInputDeviceId_t szDevices[SIR_INPUT_DEVICE_CAPACITY] = {};
	const uint32_t nDirtyCount = ( uint32_t )state.range( 0 );
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		state.SkipWithError( "Failed to initialize input service" );
		return;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( !input.connect( { i + 1, i + 1, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &szDevices[i] ) ) {
			state.SkipWithError( "Could not register controller" );
			return;
		}
	}
	if ( !input.publish() ) {
		state.SkipWithError( "Could not publish initial snapshot" );
		return;
	}
	uint64_t nTick = 2;
	for ( auto _ : state ) {
		( void )_;
		if ( !input.begin( nTick ) ) {
			state.SkipWithError( "Could not begin snapshot" );
			return;
		}
		const sirInputControlChange_t change = { SIR_INPUT_BUTTON, 17, ( nTick & 1 ) != 0 ? 1 : 0 };
		for ( uint32_t i = 0; i < nDirtyCount; ++i ) {
			if ( !input.emitControls( SIR_INPUT_TEST, szDevices[i], &change, 1, nTick * 100 ) ) {
				state.SkipWithError( "Could not write controller transition" );
				return;
			}
		}
		if ( !input.publish() ) {
			state.SkipWithError( "Could not publish snapshot" );
			return;
		}
		benchmark::DoNotOptimize( input.frame()->nCount );
		++nTick;
	}
	state.counters["dirty_devices"] = ( double )nDirtyCount;
	state.counters["frame_bytes"] = ( double )sizeof( sirInputFrame_t );
	state.counters["service_bytes"] = ( double )sizeof( SIRInputService );
}

BENCHMARK( SIRBenchmarkInputSnapshotDirtyDevices )->Arg( 0 )->Arg( 1 )->Arg( 8 )->Arg( 32 );

static void SIRBenchmarkInputTimestampMetrics( benchmark::State& state )
{
	SIRInputService input;
	SIRInputSnapshotBenchmarkBackend backend;
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		state.SkipWithError( "Unable to initialize input service" );
		return;
	}
	sirInputDeviceId_t id = {};
	if ( !input.connect( { 1, 1, SIR_INPUT_TEST, SIR_INPUT_POINTER }, &id ) || !input.publish() ) {
		state.SkipWithError( "Unable to connect pointer" );
		return;
	}
	const bool bTimingEnabled = state.range( 0 ) != 0;
	input.setTimingMetricsEnabled( bTimingEnabled );
	uint64_t nTick = 2;
	for ( auto _ : state ) {
		( void )_;
		if ( !input.begin( nTick ) || !input.emitPointer( SIR_INPUT_TEST, id, SIR_INPUT_MOTION, 0, 1, -1 ) || !input.publish() ) {
			state.SkipWithError( "Unable to publish timestamped input" );
			return;
		}
		benchmark::DoNotOptimize( input.frame()->nCutoffNs );
		++nTick;
	}
	state.counters["timing_enabled"] = bTimingEnabled ? 1.0 : 0.0;
	state.counters["event_bytes"] = ( double )sizeof( sirInputEvent_t );
}

BENCHMARK( SIRBenchmarkInputTimestampMetrics )->Arg( 0 )->Arg( 1 );
