#include <Core/Platform/Window/SIRWindowService.h>

SIRWindowService::SIRWindowService( SIRWindowSystem& windows, SIRWindowDriver& driver, const sirWindowDesc_t& desc,
                                    SIRInputService *pInput ) noexcept
    : m_Windows( windows ), m_Driver( driver ), m_Desc( desc ), m_pInput( pInput ), m_pDesktop( nullptr ), m_hPrimary{}, m_bStarted( false )
{
}

bool SIRWindowService::setDesktopBackend( SIRInputBackend& backend ) noexcept
{
	if ( m_bStarted || m_pInput == nullptr || m_pDesktop != nullptr ) {
		return false;
	}
	m_pDesktop = &backend;
	return true;
}

sirSystemResult_t SIRWindowService::initialize( void ) noexcept
{
	if ( m_bStarted || ( m_pInput != nullptr && m_pDesktop == nullptr ) ) {
		return SIR_SYSTEM_ERROR;
	}
	if ( m_Windows.initialize( m_Driver ) != SIR_WINDOW_OK ) {
		return SIR_SYSTEM_ERROR;
	}
	sirWindowHandle_t hPrimary = {};
	if ( m_Windows.create( m_Desc, &hPrimary ) != SIR_WINDOW_OK ) {
		( void )m_Windows.shutdown();
		return SIR_SYSTEM_ERROR;
	}
	if ( m_pInput != nullptr && m_Windows.bindInput( hPrimary, *m_pInput, *m_pDesktop ) != SIR_WINDOW_OK ) {
		( void )m_Windows.shutdown();
		return SIR_SYSTEM_ERROR;
	}
	m_hPrimary = hPrimary;
	m_bStarted = true;
	return SIR_SYSTEM_OK;
}

sirSystemResult_t SIRWindowService::shutdown( void ) noexcept
{
	if ( !m_bStarted ) {
		return SIR_SYSTEM_OK;
	}
	const sirWindowResult_t eResult = m_Windows.shutdown();
	if ( eResult != SIR_WINDOW_OK ) {
		return eResult == SIR_WINDOW_BUSY ? SIR_SYSTEM_BUSY : SIR_SYSTEM_ERROR;
	}
	m_hPrimary = {};
	m_bStarted = false;
	return SIR_SYSTEM_OK;
}

sirWindowHandle_t SIRWindowService::primaryWindow( void ) const noexcept
{
	return m_hPrimary;
}

sirWindowResult_t SIRWindowService::pump( void ) noexcept
{
	return m_bStarted ? m_Windows.pump() : SIR_WINDOW_INVALID;
}

bool SIRWindowService::hasLiveWindows( void ) const noexcept
{
	return m_bStarted && m_Windows.hasLiveWindows();
}
