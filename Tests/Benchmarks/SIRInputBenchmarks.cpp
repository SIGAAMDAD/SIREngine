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

#include <Core/Platform/Input/SIRInputControllerProfiles.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <benchmark/benchmark.h>

class SIRBenchmarkInputBackend final : public SIRInputBackend
{
public:
	sirInputBackendId_t id( void ) const noexcept override
	{
		return SIR_INPUT_TEST;
	}

	void sample( SIRInputService &service ) noexcept override
	{
		( void )service;
	}

	void resynchronize( SIRInputService &service ) noexcept override
	{
		( void )service;
	}

	bool vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )id;
		( void )nLeft;
		( void )nRight;
		return true;
	}

	void stop( SIRInputService &service ) noexcept override
	{
		( void )service;
	}
};

static bool initializeInputBenchmark( SIRInputService &input, SIRBenchmarkInputBackend &backend, uint32_t nDevices,
                                      sirInputDeviceId_t *pDevices ) noexcept
{
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < nDevices; ++i ) {
		const sirInputHardwareKey_t key = { 10000 + i, 20000 + i, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER };
		if ( !input.connect( key, &pDevices[i] ) ) {
			return false;
		}
	}
	return input.publish();
}

static void SIRBenchmarkInputFramePublish( benchmark::State &state )
{
	SIRInputService input;
	SIRBenchmarkInputBackend backend;
	sirInputDeviceId_t devices[SIR_INPUT_DEVICE_CAPACITY] = {};
	const uint32_t nDevices = ( uint32_t )state.range( 0 );
	if ( !initializeInputBenchmark( input, backend, nDevices, devices ) ) {
		state.SkipWithError( "Cannot initialize input frame benchmark" );
		return;
	}
	uint64_t nTick = 2;
	for ( auto _ : state ) {
		( void )_;
		if ( !input.begin( nTick++ ) || !input.publish() ) {
			state.SkipWithError( "Input frame publication failed" );
			break;
		}
		benchmark::DoNotOptimize( input.frame() );
	}
	state.counters["service_bytes"] = ( double )sizeof( SIRInputService );
	state.counters["frame_bytes"] = ( double )sizeof( sirInputFrame_t );
}

static void SIRBenchmarkInputControlBatch( benchmark::State &state )
{
	SIRInputService input;
	SIRBenchmarkInputBackend backend;
	sirInputDeviceId_t device = {};
	const uint32_t nChanges = ( uint32_t )state.range( 0 );
	if ( !initializeInputBenchmark( input, backend, 1, &device ) ) {
		state.SkipWithError( "Cannot initialize input batch benchmark" );
		return;
	}
	sirInputControlChange_t changes[128] = {};
	for ( uint32_t i = 0; i < nChanges; ++i ) {
		changes[i] = { SIR_INPUT_BUTTON, 31, ( i & 1 ) == 0 ? 1 : 0 };
	}
	if ( ( nChanges & 1 ) != 0 ) {
		changes[0].nValue = 0;
	}
	uint64_t nTick = 2;
	for ( auto _ : state ) {
		( void )_;
		if ( !input.begin( nTick++ ) ) {
			state.SkipWithError( "Cannot begin input batch tick" );
			break;
		}
		if ( ( nChanges & 1 ) != 0 ) {
			changes[0].nValue ^= 1;
		}
		if ( !input.emitControls( SIR_INPUT_TEST, device, changes, nChanges, nTick ) || !input.publish() ) {
			state.SkipWithError( "Input batch failed" );
			break;
		}
		benchmark::DoNotOptimize( input.frame()->nCount );
	}
	state.SetItemsProcessed( state.iterations() * ( int64_t )nChanges );
}

static void SIRBenchmarkInputPointerSamples( benchmark::State &state )
{
	SIRInputService input;
	SIRBenchmarkInputBackend backend;
	sirInputDeviceId_t device = {};
	if ( !input.attach( backend ) || !input.begin( 1 ) || !input.connect( { 1, 700, SIR_INPUT_TEST, SIR_INPUT_POINTER }, &device ) ||
	     !input.publish() ) {
		state.SkipWithError( "Cannot initialize pointer benchmark" );
		return;
	}
	const uint32_t nSamples = ( uint32_t )state.range( 0 );
	uint64_t nTick = 2;
	for ( auto _ : state ) {
		( void )_;
		if ( !input.begin( nTick++ ) ) {
			state.SkipWithError( "Cannot begin pointer tick" );
			break;
		}
		for ( uint32_t i = 0; i < nSamples; ++i ) {
			if ( !input.emitPointer( SIR_INPUT_TEST, device, SIR_INPUT_MOTION, 0, 1, -1, i + 1 ) ) {
				state.SkipWithError( "Pointer sample failed" );
				return;
			}
		}
		if ( !input.publish() ) {
			state.SkipWithError( "Cannot publish pointer tick" );
			break;
		}
		benchmark::DoNotOptimize( input.frame()->nCount );
	}
	state.SetItemsProcessed( state.iterations() * ( int64_t )nSamples );
}

static void SIRBenchmarkInputResolveDevice( benchmark::State &state )
{
	SIRInputService input;
	SIRBenchmarkInputBackend backend;
	sirInputDeviceId_t devices[SIR_INPUT_DEVICE_CAPACITY] = {};
	const uint32_t nDevices = ( uint32_t )state.range( 0 );
	if ( !initializeInputBenchmark( input, backend, nDevices, devices ) ) {
		state.SkipWithError( "Cannot initialize device resolution benchmark" );
		return;
	}
	sirInputDeviceId_t found = {};
	for ( auto _ : state ) {
		( void )_;
		const bool bFound = input.findDevice( SIR_INPUT_TEST, 20000 + nDevices - 1, &found );
		benchmark::DoNotOptimize( bFound );
		benchmark::DoNotOptimize( found.nGeneration );
	}
	state.counters["active_devices"] = ( double )nDevices;
}

static void SIRBenchmarkInputControllerProfileLookup( benchmark::State &state )
{
	SIRInputControllerProfiles profiles;
	const uint32_t nProfiles = ( uint32_t )state.range( 0 );
	sirInputControllerProfile_t profile = {};
	profile.nProduct = 0x1000;
	for ( uint32_t i = 0; i < nProfiles; ++i ) {
		profile.nVendor = ( uint16_t )( 0x2000 + i );
		if ( !profiles.add( profile ) ) {
			state.SkipWithError( "Cannot initialize controller profile benchmark" );
			return;
		}
	}
	for ( auto _ : state ) {
		( void )_;
		const sirInputControllerProfile_t *pProfile = profiles.find( 3, ( uint16_t )( 0x2000 + nProfiles - 1 ), 0x1000 );
		benchmark::DoNotOptimize( pProfile );
	}
	state.counters["profile_count"] = ( double )nProfiles;
}

static void SIRBenchmarkInputConnectDisconnect( benchmark::State &state )
{
	SIRInputService input;
	SIRBenchmarkInputBackend backend;
	if ( !input.attach( backend ) ) {
		state.SkipWithError( "Cannot attach input backend" );
		return;
	}
	uint64_t nTick = 1;
	for ( auto _ : state ) {
		( void )_;
		sirInputDeviceId_t device = {};
		if ( !input.begin( nTick++ ) || !input.connect( { 0, 1, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &device ) ||
		     !input.disconnect( SIR_INPUT_TEST, device ) || !input.publish() ) {
			state.SkipWithError( "Device lifecycle benchmark failed" );
			break;
		}
		benchmark::DoNotOptimize( input.frame()->nCount );
	}
	state.SetItemsProcessed( state.iterations() );
}

BENCHMARK( SIRBenchmarkInputFramePublish )->Arg( 0 )->Arg( 1 )->Arg( 8 )->Arg( 32 );
BENCHMARK( SIRBenchmarkInputControlBatch )->Arg( 1 )->Arg( 2 )->Arg( 8 )->Arg( 32 )->Arg( 128 );
BENCHMARK( SIRBenchmarkInputPointerSamples )->Arg( 125 )->Arg( 500 )->Arg( 1000 )->Arg( 2000 );
BENCHMARK( SIRBenchmarkInputResolveDevice )->Arg( 1 )->Arg( 8 )->Arg( 32 );
BENCHMARK( SIRBenchmarkInputControllerProfileLookup )->Arg( 1 )->Arg( 8 )->Arg( 32 );
BENCHMARK( SIRBenchmarkInputConnectDisconnect );
