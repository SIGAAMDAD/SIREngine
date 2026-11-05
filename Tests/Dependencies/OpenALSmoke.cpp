#include <AL/al.h>

int main( void )
{
	const ALenum eError = alGetError();
	return eError == AL_OUT_OF_MEMORY ? 1 : 0;
}
