#include <Core/Platform/Window/Headless/SIRWindowDriverHeadless.h>

SIRWindowResult SIRWindowDriverHeadless::initialize( void ) noexcept
{
	if ( m_bInitialized ) {
		return SIRWindowResult::SIR_WINDOW_BUSY;
	}
	m_bInitialized = true;
	return SIRWindowResult::SIR_WINDOW_OK;
}

SIRWindowResult SIRWindowDriverHeadless::createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                         sirWindowNativeView_t *pView ) noexcept
{
	static_cast<void>(desc);
	static_cast<void>(nSlot);
	static_cast<void>(pView);
	return m_bInitialized ? SIRWindowResult::SIR_WINDOW_UNSUPPORTED : SIRWindowResult::SIR_WINDOW_STOPPED;
}

SIRWindowResult SIRWindowDriverHeadless::destroyWindow( uint32_t nSlot ) noexcept
{
	static_cast<void>(nSlot);
	return m_bInitialized ? SIRWindowResult::SIR_WINDOW_INVALID_HANDLE : SIRWindowResult::SIR_WINDOW_STOPPED;
}

SIRWindowResult SIRWindowDriverHeadless::mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept
{
	static_cast<void>(nSlot);
	static_cast<void>(mutation);
	return m_bInitialized ? SIRWindowResult::SIR_WINDOW_UNSUPPORTED : SIRWindowResult::SIR_WINDOW_STOPPED;
}

SIRWindowResult SIRWindowDriverHeadless::pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	static_cast<void>(nBudget);
	static_cast<void>(pCallback);
	static_cast<void>(pContext);
	return m_bInitialized ? SIRWindowResult::SIR_WINDOW_OK : SIRWindowResult::SIR_WINDOW_STOPPED;
}

void SIRWindowDriverHeadless::shutdown( void ) noexcept
{
	m_bInitialized = false;
}
