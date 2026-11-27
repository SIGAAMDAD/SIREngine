#include "SIRInputBindingMap.h"

SIRInputBindingMap::SIRInputBindingMap( void ) noexcept
{
}

SIRInputBindingMap::~SIRInputBindingMap() noexcept
{
}

bool SIRInputBindingMap::addBind( const SIRInputBinding& binding ) noexcept
{
	return false;
}

bool SIRInputBindingMap::removeBind( const SIRStringAtom& bindName ) noexcept
{
	return false;
}

const SIRInputBinding *SIRInputBindingMap::getBind( const SIRStringAtom& bindName ) const noexcept
{
	return nullptr;
}
