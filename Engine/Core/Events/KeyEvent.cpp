#include "KeyEvent.h"

const KeyNum_t SDLToKeynumTable[ NumKeyIds ] = {
	{  }
};

KeyNum_t CKeyEvent::SDLKeyToEngineKey( SDL_Keysym *pKeysym )
{
	KeyNum_t key = 0;
	if ( pKeysym->sym >= SDLK_0 && pKeysym->sym <= SDLK_9 ) {
		key = Key_0 + ( pKeysym->sym - SDLK_0 );
	}
	else if ( pKeysym->sym >= SDLK_a && pKeysym->sym <= SDLK_z ) {
		key = Key_A + ( pKeysym->sym - SDLK_a );
	}
	else if ( ( SDL_GetModState() & KMOD_NUM ) == KMOD_NUM && ( pKeysym->sym >= SDLK_KP_1 && pKeysym->sym <= SDLK_KP_0 )) {
		// numlock is on
		key = Key_KP_0 + ( pKeysym->sym - SDLK_KP_1 );
	}
	else {
		switch ( pKeysym->sym ) {
		case SDLK_LSHIFT:
		case SDLK_RSHIFT:
			return Key_Shift;
		case SDLK_LCTRL:
		case SDLK_RCTRL:
			break;
		case SDLK_LALT:
		case SDLK_RALT:
			break;
		case SDLK_BACKSPACE:
		case SDLK_SPACE:
		case SDLK_BACKSLASH:
		default:
			break;
		};
	}
	if ( pKeysym->sym == SDLK_BACKQUOTE ) {
		// console key, cannot be rebound
		key = Key_Console;
	}

	return key;
}