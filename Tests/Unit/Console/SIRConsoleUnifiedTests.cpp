#include <Core/Console/SIRConsoleSystem.h>

#include <cassert>

struct sirConsoleTestCommandOwner_t
{
	uint32_t nCalls{};

	SIRConsoleResult execute( SIRSpan<const sirStringView_t> arguments ) noexcept
	{
		++nCalls;
		return arguments.getSize() == 1 ? SIRConsoleResult::SIR_CONSOLE_OK : SIRConsoleResult::SIR_CONSOLE_INVALID;
	}
};

int main( void )
{
	SIRStringPool pool;
	sirConsoleSystemConfig_t config{};
	config.pPool = &pool;
	config.nCVarCapacity = 8;
	config.nOverrideCapacity = 8;
	config.nCommandCapacity = 4;
	config.nCommandBytes = 64;
	SIRConsoleSystem console( config );
	assert( console.initialize() == SIRSystemResult::SIR_SYSTEM_OK );

	sirCVarHandle_t handle{};
	assert( console.registerCVar<uint32_t>( SIRStringAtom( 1 ), 2U, SIR_CVAR_FLAG_ARCHIVE, handle ) == SIRConsoleResult::SIR_CONSOLE_OK );
	uint32_t nValue = 0;
	assert( console.getCVar<uint32_t>( handle, nValue ) == SIRConsoleResult::SIR_CONSOLE_OK );
	assert( nValue == 2 );
	assert( console.setCVar<uint32_t>( handle, 4U, SIRCVarSource::SIR_CVAR_SESSION_CONSOLE, 1 ) == SIRConsoleResult::SIR_CONSOLE_OK );
	assert( console.getCVar<uint32_t>( handle, nValue ) == SIRConsoleResult::SIR_CONSOLE_OK );
	assert( nValue == 4 );

	sirConsoleTestCommandOwner_t owner{};
	sirCommandHandle_t command{};
	const SIRConsoleResult eCommand = console.registerCommand<sirConsoleTestCommandOwner_t, &sirConsoleTestCommandOwner_t::execute>( SIRStringAtom( 2 ), owner, command );
	assert( eCommand == SIRConsoleResult::SIR_CONSOLE_OK );
	assert( console.unregisterCVar( handle ) == SIRConsoleResult::SIR_CONSOLE_OK );
	assert( console.getCVar<uint32_t>( handle, nValue ) == SIRConsoleResult::SIR_CONSOLE_STALE_HANDLE );
	assert( console.shutdown() == SIRSystemResult::SIR_SYSTEM_OK );
	return 0;
}
