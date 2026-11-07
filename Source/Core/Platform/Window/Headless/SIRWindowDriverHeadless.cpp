#include <Core/Platform/Window/Headless/SIRWindowDriverHeadless.h>

sirWindowResult_t SIRWindowDriverHeadless::initialize( void ) noexcept
{
	if ( m_bInitialized ) {
		return SIR_WINDOW_BUSY;
	}
	m_bInitialized = true;
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverHeadless::createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                         sirWindowNativeView_t *pView ) noexcept
{
	( void )desc;
	( void )nSlot;
	( void )pView;
	return m_bInitialized ? SIR_WINDOW_UNSUPPORTED : SIR_WINDOW_STOPPED;
}

sirWindowResult_t SIRWindowDriverHeadless::destroyWindow( uint32_t nSlot ) noexcept
{
	( void )nSlot;
	return m_bInitialized ? SIR_WINDOW_INVALID_HANDLE : SIR_WINDOW_STOPPED;
}

sirWindowResult_t SIRWindowDriverHeadless::pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	( void )nBudget;
	( void )pCallback;
	( void )pContext;
	return m_bInitialized ? SIR_WINDOW_OK : SIR_WINDOW_STOPPED;
}

void SIRWindowDriverHeadless::shutdown( void ) noexcept
{
	m_bInitialized = false;
}
