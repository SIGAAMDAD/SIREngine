#include <Core/Platform/Headless/SIRWindowDriverHeadless.h>

sirWindowResult SIRWindowDriverHeadless::initialize( void ) noexcept
{
	if ( m_bInitialized ) {
		return sirWindowResult::SIR_WINDOW_BUSY;
	}
	m_bInitialized = true;
	return sirWindowResult::SIR_WINDOW_OK;
}

sirWindowResult SIRWindowDriverHeadless::createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                         sirWindowNativeView_t *pView ) noexcept
{
	static_cast<void>(desc);
	static_cast<void>(nSlot);
	static_cast<void>(pView);
	return m_bInitialized ? sirWindowResult::SIR_WINDOW_UNSUPPORTED : sirWindowResult::SIR_WINDOW_STOPPED;
}

sirWindowResult SIRWindowDriverHeadless::destroyWindow( uint32_t nSlot ) noexcept
{
	static_cast<void>(nSlot);
	return m_bInitialized ? sirWindowResult::SIR_WINDOW_INVALID_HANDLE : sirWindowResult::SIR_WINDOW_STOPPED;
}

sirWindowResult SIRWindowDriverHeadless::mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept
{
	static_cast<void>(nSlot);
	static_cast<void>(mutation);
	return m_bInitialized ? sirWindowResult::SIR_WINDOW_UNSUPPORTED : sirWindowResult::SIR_WINDOW_STOPPED;
}

sirWindowResult SIRWindowDriverHeadless::pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	static_cast<void>(nBudget);
	static_cast<void>(pCallback);
	static_cast<void>(pContext);
	return m_bInitialized ? sirWindowResult::SIR_WINDOW_OK : sirWindowResult::SIR_WINDOW_STOPPED;
}

void SIRWindowDriverHeadless::shutdown( void ) noexcept
{
	m_bInitialized = false;
}
