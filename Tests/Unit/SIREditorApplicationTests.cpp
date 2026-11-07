#include <Editor/Host/SIREditorApplication.h>
#include <assert.h>
#include <stdio.h>

typedef struct sirEditorAppTestContext_s
{
	SIREditorApplication *pApp;
	uint32_t nCalls;
	bool bFail;
} sirEditorAppTestContext_t;

static bool sirTestEditorFrame( uint64_t nFrame, void *pContext ) noexcept
{
	sirEditorAppTestContext_t *pState = ( sirEditorAppTestContext_t * )pContext;
	assert( nFrame == pState->nCalls );
	++pState->nCalls;
	if ( pState->nCalls == 3 ) {
		pState->pApp->requestQuit();
	}
	return !pState->bFail;
}

int main( void )
{
	SIREditorApplication app;
	assert( app.tick() == SIR_EDITOR_APP_NOT_INITIALIZED );
	assert( app.initialize( nullptr, nullptr ) == SIR_EDITOR_APP_INVALID_ARGUMENT );
	sirEditorAppTestContext_t state = { &app, 0, false };
	assert( app.initialize( sirTestEditorFrame, &state ) == SIR_EDITOR_APP_OK );
	assert( app.initialize( sirTestEditorFrame, &state ) == SIR_EDITOR_APP_ALREADY_INITIALIZED );
	assert( app.run( 0 ) == SIR_EDITOR_APP_INVALID_ARGUMENT );
	assert( app.run( 20 ) == SIR_EDITOR_APP_OK );
	assert( state.nCalls == 3 && app.getFrameCount() == 3 && !app.isRunning() );
	assert( app.tick() == SIR_EDITOR_APP_OK && state.nCalls == 3 );
	app.shutdown();
	assert( app.run( 1 ) == SIR_EDITOR_APP_NOT_INITIALIZED );
	state.nCalls = 0;
	state.bFail = true;
	assert( app.initialize( sirTestEditorFrame, &state ) == SIR_EDITOR_APP_OK );
	assert( app.run( 5 ) == SIR_EDITOR_APP_CALLBACK_FAILED && state.nCalls == 1 );
	app.shutdown();
	puts( "PASS: native editor application loop" );
	return 0;
}
