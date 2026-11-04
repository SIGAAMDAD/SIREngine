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

#include <Core/Platform/Input/Windows/SIRWindowsGameInputBackend.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <GameInput.h>

static_assert( GAMEINPUT_API_VERSION == 1, "SIREngine GameInput backend targets the pinned native v1 SDK" );
static_assert( GameInput::v1::GameInputGamepadA == 0x00000004, "GameInput buttons changed" );
static_assert( GameInput::v1::GameInputGamepadDPadRight == 0x00000200, "GameInput D-pad changed" );

SIRWindowsGameInputBackend::SIRWindowsGameInputBackend( void ) noexcept
    : m_pLibrary( nullptr ), m_pInput( nullptr ), m_pCursor( nullptr ), m_nDeviceCallback( 0 ), m_Devices{}, m_Pending{},
      m_bOverflow( false )
{
	for ( uint32_t i = 0; i < PENDING_CAPACITY; ++i ) {
		m_Pending[i].store( nullptr, std::memory_order_relaxed );
	}
}

sirInputBackendId_t SIRWindowsGameInputBackend::id( void ) const noexcept
{
	return SIR_INPUT_WIN_GAMEINPUT;
}

bool SIRWindowsGameInputBackend::initialized( void ) const noexcept
{
	return m_pInput != nullptr;
}

void CALLBACK SIRWindowsGameInputBackend::onDeviceChanged( GameInput::v1::GameInputCallbackToken nToken, void *pContext,
                                                           GameInput::v1::IGameInputDevice *pDevice, uint64_t nTimestamp,
                                                           GameInput::v1::GameInputDeviceStatus eCurrent,
                                                           GameInput::v1::GameInputDeviceStatus ePrevious ) noexcept
{
	( void )nToken;
	( void )nTimestamp;
	( void )eCurrent;
	( void )ePrevious;
	SIRWindowsGameInputBackend *pBackend = ( SIRWindowsGameInputBackend * )pContext;
	if ( pBackend != nullptr && pDevice != nullptr ) {
		pBackend->enqueueDevice( pDevice );
	}
}

void SIRWindowsGameInputBackend::enqueueDevice( void *pDevice ) noexcept
{
	GameInput::v1::IGameInputDevice *pNative = ( GameInput::v1::IGameInputDevice * )pDevice;
	pNative->AddRef();
	for ( uint32_t i = 0; i < PENDING_CAPACITY; ++i ) {
		void *pExpected = nullptr;
		if ( m_Pending[i].compare_exchange_strong( pExpected, pNative, std::memory_order_release, std::memory_order_relaxed ) ) {
			return;
		}
	}
	pNative->Release();
	m_bOverflow.store( true, std::memory_order_release );
}

bool SIRWindowsGameInputBackend::initialize( void ) noexcept
{
	if ( m_pInput != nullptr || m_pLibrary != nullptr ) {
		return false;
	}
	HMODULE hDLL = LoadLibraryExW( L"GameInput.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
	if ( hDLL == nullptr ) {
		return false;
	}
	typedef HRESULT( WINAPI * sirGameInputCreateFn_t )( IUnknown ** );
	const sirGameInputCreateFn_t pCreate = ( sirGameInputCreateFn_t )GetProcAddress( hDLL, "GameInputCreate" );
	IUnknown *pLegacy = nullptr;
	GameInput::v1::IGameInput *pInput = nullptr;
	if ( pCreate == nullptr || FAILED( pCreate( &pLegacy ) ) || pLegacy == nullptr ||
	     FAILED( pLegacy->QueryInterface( GameInput::v1::IID_IGameInput, ( void ** )&pInput ) ) ) {
		if ( pLegacy != nullptr ) {
			pLegacy->Release();
		}
		FreeLibrary( hDLL );
		return false;
	}
	pLegacy->Release();
	m_pLibrary = hDLL;
	m_pInput = pInput;
	const HRESULT nStatus = pInput->RegisterDeviceCallback(
	    nullptr, GameInput::v1::GameInputKindGamepad, GameInput::v1::GameInputDeviceConnected, GameInput::v1::GameInputBlockingEnumeration,
	    this, &onDeviceChanged, ( GameInput::v1::GameInputCallbackToken * )&m_nDeviceCallback );
	if ( FAILED( nStatus ) ) {
		pInput->Release();
		m_pInput = nullptr;
		FreeLibrary( hDLL );
		m_pLibrary = nullptr;
		return false;
	}
	return true;
}

int32_t SIRWindowsGameInputBackend::findDevice( const void *pDevice ) const noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( m_Devices[i].pDevice == pDevice ) {
			return ( int32_t )i;
		}
	}
	return -1;
}

bool SIRWindowsGameInputBackend::addDevice( SIRInputService &service, void *pDevice ) noexcept
{
	if ( findDevice( pDevice ) >= 0 ) {
		return true;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( m_Devices[i].pDevice == nullptr ) {
			sirInputDeviceId_t id = {};
			const uint64_t nNative = ( uint64_t )( uintptr_t )pDevice;
			if ( !service.connect( { 0, nNative, SIR_INPUT_WIN_GAMEINPUT, SIR_INPUT_CONTROLLER }, &id ) ) {
				return false;
			}
			( ( GameInput::v1::IGameInputDevice * )pDevice )->AddRef();
			m_Devices[i].pDevice = pDevice;
			m_Devices[i].id = id;
			m_Devices[i].decoded = {};
			return true;
		}
	}
	return false;
}

void SIRWindowsGameInputBackend::dropDevice( SIRInputService &service, uint32_t iSlot ) noexcept
{
	if ( m_Devices[iSlot].pDevice == nullptr ) {
		return;
	}
	( void )service.disconnect( id(), m_Devices[iSlot].id );
	( ( GameInput::v1::IGameInputDevice * )m_Devices[iSlot].pDevice )->Release();
	m_Devices[iSlot] = {};
}

void SIRWindowsGameInputBackend::consumeDeviceChanges( SIRInputService &service ) noexcept
{
	for ( uint32_t i = 0; i < PENDING_CAPACITY; ++i ) {
		GameInput::v1::IGameInputDevice *pDevice =
		    ( GameInput::v1::IGameInputDevice * )m_Pending[i].exchange( nullptr, std::memory_order_acquire );
		if ( pDevice == nullptr ) {
			continue;
		}
		const bool bConnected = ( pDevice->GetDeviceStatus() & GameInput::v1::GameInputDeviceConnected ) != 0;
		const int32_t iSlot = findDevice( pDevice );
		if ( bConnected && iSlot < 0 ) {
			( void )addDevice( service, pDevice );
		} else if ( !bConnected && iSlot >= 0 ) {
			dropDevice( service, ( uint32_t )iSlot );
		}
		pDevice->Release();
	}
}

bool SIRWindowsGameInputBackend::processReading( SIRInputService &service, void *pReading ) noexcept
{
	GameInput::v1::IGameInputReading *pNative = ( GameInput::v1::IGameInputReading * )pReading;
	GameInput::v1::IGameInputDevice *pDevice = nullptr;
	pNative->GetDevice( &pDevice );
	if ( pDevice == nullptr ) {
		return true;
	}
	if ( ( pDevice->GetDeviceStatus() & GameInput::v1::GameInputDeviceConnected ) == 0 ) {
		pDevice->Release();
		return true;
	}
	if ( !addDevice( service, pDevice ) ) {
		pDevice->Release();
		return false;
	}
	const int32_t iSlot = findDevice( pDevice );
	pDevice->Release();
	if ( iSlot < 0 ) {
		return false;
	}
	GameInput::v1::GameInputGamepadState nativeState = {};
	if ( !pNative->GetGamepadState( &nativeState ) ) {
		return true;
	}
	const sirGameInputRawGamepad_t raw = { ( uint32_t )nativeState.buttons, nativeState.leftThumbstickX,  nativeState.leftThumbstickY,
	                                       nativeState.rightThumbstickX,    nativeState.rightThumbstickY, nativeState.leftTrigger,
	                                       nativeState.rightTrigger };
	sirGameInputDecodedGamepad_t next = {};
	sirInputControlChange_t changes[SIRWindowsGameInputCodec::MAX_CHANGES] = {};
	uint32_t nCount = 0;
	if ( !SIRWindowsGameInputCodec::decode( raw, m_Devices[iSlot].decoded, &next, changes, SIRWindowsGameInputCodec::MAX_CHANGES,
	                                        &nCount ) ) {
		return false;
	}
	if ( nCount != 0 ) {
		const uint64_t nSourceNs = SIRWindowsGameInputCodec::timestampNanoseconds( pNative->GetTimestamp() );
		if ( !service.emitControls( id(), m_Devices[iSlot].id, changes, nCount, nSourceNs, SIR_INPUT_CLOCK_GAMEINPUT ) ) {
			return false;
		}
	}
	m_Devices[iSlot].decoded = next;
	return true;
}

void SIRWindowsGameInputBackend::invalidateHistory( SIRInputService &service ) noexcept
{
	if ( m_pCursor != nullptr ) {
		( ( GameInput::v1::IGameInputReading * )m_pCursor )->Release();
		m_pCursor = nullptr;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( m_Devices[i].pDevice != nullptr ) {
			m_Devices[i].decoded.bValid = false;
			( void )service.loseState( id(), m_Devices[i].id );
		}
	}
}

void SIRWindowsGameInputBackend::refreshEnumeration( void ) noexcept
{
	GameInput::v1::IGameInput *pInput = ( GameInput::v1::IGameInput * )m_pInput;
	if ( pInput == nullptr ) {
		return;
	}
	if ( m_nDeviceCallback != 0 ) {
		pInput->UnregisterCallback( m_nDeviceCallback );
		m_nDeviceCallback = 0;
	}
	( void )pInput->RegisterDeviceCallback( nullptr, GameInput::v1::GameInputKindGamepad, GameInput::v1::GameInputDeviceConnected,
	                                        GameInput::v1::GameInputBlockingEnumeration, this, &onDeviceChanged,
	                                        ( GameInput::v1::GameInputCallbackToken * )&m_nDeviceCallback );
}

void SIRWindowsGameInputBackend::sample( SIRInputService &service ) noexcept
{
	GameInput::v1::IGameInput *pInput = ( GameInput::v1::IGameInput * )m_pInput;
	if ( pInput == nullptr ) {
		return;
	}
	if ( m_bOverflow.exchange( false, std::memory_order_acq_rel ) ) {
		refreshEnumeration();
	}
	consumeDeviceChanges( service );
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		GameInput::v1::IGameInputDevice *pDevice = ( GameInput::v1::IGameInputDevice * )m_Devices[i].pDevice;
		if ( pDevice != nullptr && ( pDevice->GetDeviceStatus() & GameInput::v1::GameInputDeviceConnected ) == 0 ) {
			dropDevice( service, i );
		}
	}
	for ( uint32_t i = 0; i < HISTORY_BUDGET; ++i ) {
		GameInput::v1::IGameInputReading *pReading = nullptr;
		const HRESULT nStatus = m_pCursor == nullptr ? pInput->GetCurrentReading( GameInput::v1::GameInputKindGamepad, nullptr, &pReading )
		                                             : pInput->GetNextReading( ( GameInput::v1::IGameInputReading * )m_pCursor,
		                                                                       GameInput::v1::GameInputKindGamepad, nullptr, &pReading );
		if ( nStatus == GameInput::v1::GAMEINPUT_E_REFERENCE_READING_TOO_OLD ) {
			invalidateHistory( service );
			continue;
		}
		if ( FAILED( nStatus ) || pReading == nullptr ) {
			break;
		}
		if ( !processReading( service, pReading ) ) {
			pReading->Release();
			break;
		}
		if ( m_pCursor != nullptr ) {
			( ( GameInput::v1::IGameInputReading * )m_pCursor )->Release();
		}
		m_pCursor = pReading;
	}
}

void SIRWindowsGameInputBackend::resynchronize( SIRInputService &service ) noexcept
{
	invalidateHistory( service );
	if ( m_pInput != nullptr ) {
		m_bOverflow.store( true, std::memory_order_release );
	}
}

bool SIRWindowsGameInputBackend::vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		sirGameInputNativeDevice_t &device = m_Devices[i];
		if ( device.pDevice != nullptr && device.id.nSlot == id.nSlot && device.id.nGeneration == id.nGeneration ) {
			device.nMotorLeft = nLeft;
			device.nMotorRight = nRight;
			const GameInput::v1::GameInputRumbleParams rumble = {
			    ( float )nLeft / 65535.0f, ( float )nRight / 65535.0f,
			    ( float )device.nTriggerLeft / 65535.0f, ( float )device.nTriggerRight / 65535.0f
			};
			( ( GameInput::v1::IGameInputDevice * )device.pDevice )->SetRumbleState( &rumble );
			return true;
		}
	}
	return false;
}

bool SIRWindowsGameInputBackend::vibrateTriggers( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		sirGameInputNativeDevice_t &device = m_Devices[i];
		if ( device.pDevice != nullptr && device.id.nSlot == id.nSlot && device.id.nGeneration == id.nGeneration ) {
			device.nTriggerLeft = nLeft;
			device.nTriggerRight = nRight;
			const GameInput::v1::GameInputRumbleParams rumble = {
			    ( float )device.nMotorLeft / 65535.0f, ( float )device.nMotorRight / 65535.0f,
			    ( float )nLeft / 65535.0f, ( float )nRight / 65535.0f
			};
			( ( GameInput::v1::IGameInputDevice * )device.pDevice )->SetRumbleState( &rumble );
			return true;
		}
	}
	return false;
}

uint32_t SIRWindowsGameInputBackend::hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		const sirGameInputNativeDevice_t &device = m_Devices[i];
		if ( device.pDevice == nullptr || device.id.nSlot != hDevice.nSlot ||
		     device.id.nGeneration != hDevice.nGeneration ) {
			continue;
		}
		const GameInput::v1::GameInputDeviceInfo *pInfo = nullptr;
		const HRESULT nResult = ( ( GameInput::v1::IGameInputDevice * )device.pDevice )->GetDeviceInfo( &pInfo );
		if ( FAILED( nResult ) || pInfo == nullptr ) {
			return SIR_INPUT_HAPTIC_CAP_NONE;
		}
		const uint32_t nMotors = ( uint32_t )pInfo->supportedRumbleMotors;
		uint32_t nCaps = SIR_INPUT_HAPTIC_CAP_NONE;
		if ( ( nMotors & ( GameInput::v1::GameInputRumbleLowFrequency | GameInput::v1::GameInputRumbleHighFrequency ) ) != 0 ) {
			nCaps |= SIR_INPUT_HAPTIC_CAP_RUMBLE;
		}
		if ( ( nMotors & ( GameInput::v1::GameInputRumbleLeftTrigger | GameInput::v1::GameInputRumbleRightTrigger ) ) != 0 ) {
			nCaps |= SIR_INPUT_HAPTIC_CAP_TRIGGER_RUMBLE;
		}
		return nCaps;
	}
	return SIR_INPUT_HAPTIC_CAP_NONE;
}

void SIRWindowsGameInputBackend::stop( SIRInputService &service ) noexcept
{
	GameInput::v1::IGameInput *pInput = ( GameInput::v1::IGameInput * )m_pInput;
	if ( pInput != nullptr && m_nDeviceCallback != 0 ) {
		pInput->UnregisterCallback( m_nDeviceCallback );
		m_nDeviceCallback = 0;
	}
	for ( uint32_t i = 0; i < PENDING_CAPACITY; ++i ) {
		GameInput::v1::IGameInputDevice *pDevice =
		    ( GameInput::v1::IGameInputDevice * )m_Pending[i].exchange( nullptr, std::memory_order_acquire );
		if ( pDevice != nullptr ) {
			pDevice->Release();
		}
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( m_Devices[i].pDevice != nullptr ) {
			( void )vibrateTriggers( m_Devices[i].id, 0, 0 );
			( void )vibrate( m_Devices[i].id, 0, 0 );
			dropDevice( service, i );
		}
	}
	if ( m_pCursor != nullptr ) {
		( ( GameInput::v1::IGameInputReading * )m_pCursor )->Release();
		m_pCursor = nullptr;
	}
	if ( pInput != nullptr ) {
		pInput->Release();
		m_pInput = nullptr;
	}
	if ( m_pLibrary != nullptr ) {
		FreeLibrary( ( HMODULE )m_pLibrary );
		m_pLibrary = nullptr;
	}
	m_bOverflow.store( false, std::memory_order_relaxed );
}
