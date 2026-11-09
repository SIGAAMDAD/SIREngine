#include <Editor/Host/SIREditorApplication.h>
#include <assert.h>
#include <stdio.h>

typedef struct sirEditorAppTestContext_s
{
	SIREditorApplication *pApp;
	uint32_t nCalls;
	bool bFail;
	bool bQuit;
} sirEditorAppTestContext_t;

static bool sirTestEditorFrame( uint64_t nFrame, void *pContext ) noexcept
{
	sirEditorAppTestContext_t *pState = ( sirEditorAppTestContext_t * )pContext;
	assert( nFrame == pState->nCalls );
	assert( pState->pApp->getCoreState() == SIR_APP_READY );
	++pState->nCalls;
	if ( pState->bQuit && pState->nCalls == 3 ) {
		pState->pApp->requestQuit();
	}
	return !pState->bFail;
}

int main( void )
{
	SIREditorApplication app;
	assert( app.getCoreState() == SIR_APP_UNINITIALIZED );
	assert( app.tick() == SIR_EDITOR_APP_NOT_INITIALIZED );
	assert( app.initialize( nullptr, nullptr ) == SIR_EDITOR_APP_INVALID_ARGUMENT );
	assert( app.getCoreState() == SIR_APP_UNINITIALIZED );

	sirEditorAppTestContext_t state = { &app, 0, false, true };
	assert( app.initialize( sirTestEditorFrame, &state ) == SIR_EDITOR_APP_OK );
	assert( app.getCoreState() == SIR_APP_READY && app.getFrameCount() == 0 && app.isRunning() );
	assert( app.initialize( sirTestEditorFrame, &state ) == SIR_EDITOR_APP_ALREADY_INITIALIZED );
	assert( app.run( 0 ) == SIR_EDITOR_APP_INVALID_ARGUMENT );
	assert( app.run( 20 ) == SIR_EDITOR_APP_OK );
	assert( state.nCalls == 3 && app.getFrameCount() == 3 && !app.isRunning() );
	assert( app.getCoreState() == SIR_APP_READY );
	assert( app.tick() == SIR_EDITOR_APP_OK && state.nCalls == 3 && app.getFrameCount() == 3 );
	app.shutdown();
	assert( app.getCoreState() == SIR_APP_STOPPED );
	assert( app.run( 1 ) == SIR_EDITOR_APP_NOT_INITIALIZED );
	// A fully shut-down editor can launch again using the same storage.
	state.nCalls = 0;
	state.bFail = true;
	state.bQuit = false;
	assert( app.initialize( sirTestEditorFrame, &state ) == SIR_EDITOR_APP_OK );
	assert( app.getCoreState() == SIR_APP_READY && app.getFrameCount() == 0 );
	assert( app.run( 5 ) == SIR_EDITOR_APP_CALLBACK_FAILED );
	assert( state.nCalls == 1 && app.getFrameCount() == 1 );
	assert( app.getCoreState() == SIR_APP_READY && !app.isRunning() );
	assert( app.tick() == SIR_EDITOR_APP_OK && state.nCalls == 1 );
	app.shutdown();
	assert( app.getCoreState() == SIR_APP_STOPPED );

	SIREditorApplication boundedApp;
	sirEditorAppTestContext_t boundedState = { &boundedApp, 0, false, false };
	assert( boundedApp.initialize( sirTestEditorFrame, &boundedState ) == SIR_EDITOR_APP_OK );
	assert( boundedApp.run( 4 ) == SIR_EDITOR_APP_OK );
	assert( boundedState.nCalls == 4 && boundedApp.getFrameCount() == 4 && boundedApp.isRunning() );
	boundedApp.requestQuit();
	assert( boundedApp.run( 4 ) == SIR_EDITOR_APP_OK && boundedState.nCalls == 4 );
	boundedApp.shutdown();
	puts( "PASS: editor frames follow core application lifecycle" );
	return 0;
}
