#include "KeyEvent.h"
#if defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
#include <GLFW/glfw3.h>
#endif

using namespace SIREngine::Events;

#if !defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
KeyNum_t CKeyEvent::SDLKeyToEngineKey( const SDL_Event& eventData )
{
	KeyNum_t key = 0;
	const SDL_Keysym *pKeysym = &eventData.key.keysym;

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
		case SDLK_RSHIFT:		key = Key_Shift; break;
		case SDLK_LCTRL:
		case SDLK_RCTRL:		key = Key_Ctrl; break;
		case SDLK_LALT:
		case SDLK_RALT:			key = Key_Alt; break;
		case SDLK_BACKSPACE:	key = Key_Backspace; break;
		case SDLK_SPACE:		key = Key_Space; break;
		case SDLK_TAB:			key = Key_Tab; break;

		case SDLK_SLASH:		key = Key_Slash; break;
		case SDLK_BACKSLASH:	key = Key_BackSlash; break;
		case SDLK_LEFTBRACKET:	key = Key_OpenBracket; break;
		case SDLK_RIGHTBRACKET:	key = Key_CloseBracket; break;

		case SDLK_PRINTSCREEN:	key = Key_Screenshot; break;
		case SDLK_HOME:			key = Key_Home; break;
		case SDLK_END:			key = Key_End; break;
		case SDLK_PAGEDOWN:		key = Key_PageDown; break;
		case SDLK_PAGEUP:		key = Key_PageUp; break;
		case SDLK_INSERT:		key = Key_Insert; break;
		case SDLK_DELETE:		key = Key_Delete; break;
		case SDLK_SCROLLLOCK:	key = Key_ScrollLock; break;
		case SDLK_CAPSLOCK:		key = Key_CapsLock;

		case SDLK_F1:			key = Key_F1; break;
		case SDLK_F2:			key = Key_F2; break;
		case SDLK_F3:			key = Key_F3; break;
		case SDLK_F4:			key = Key_F4; break;
		case SDLK_F5:			key = Key_F5; break;
		case SDLK_F6:			key = Key_F6; break;
		case SDLK_F7:			key = Key_F7; break;
		case SDLK_F8:			key = Key_F8; break;
		case SDLK_F9:			key = Key_F9; break;
		case SDLK_F10:			key = Key_F10; break;
		case SDLK_F11:			key = Key_F11; break;
		case SDLK_F12:			key = Key_F12; break;
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
#else
KeyNum_t CKeyEvent::GLFW3KeyToEngineKey( int key, int mods )
{
	KeyNum_t keyNum = 0;
	if ( key >= GLFW_KEY_0 && key <= GLFW_KEY_9 ) {
		keyNum = Key_0 + ( key + GLFW_KEY_0 );
	}
	if ( key >= GLFW_KEY_A && key <= GLFW_KEY_Z ) {
		keyNum = Key_A + ( key - GLFW_KEY_A );
	}
	else {
		switch ( key ) {
		case GLFW_KEY_LEFT_SHIFT:
		case GLFW_KEY_RIGHT_SHIFT:		keyNum = Key_Shift; break;
		case GLFW_KEY_LEFT_CONTROL:
		case GLFW_KEY_RIGHT_CONTROL:	keyNum = Key_Ctrl; break;
		case GLFW_KEY_LEFT_ALT:
		case GLFW_KEY_RIGHT_ALT:		keyNum = Key_Alt; break;
		case GLFW_KEY_BACKSPACE:		keyNum = Key_Backspace; break;
		case GLFW_KEY_SPACE:			keyNum = Key_Space; break;
		case GLFW_KEY_TAB:				keyNum = Key_Tab; break;

		case GLFW_KEY_SLASH:			keyNum = Key_Slash; break;
		case GLFW_KEY_BACKSLASH:		keyNum = Key_BackSlash; break;
		case GLFW_KEY_LEFT_BRACKET:		keyNum = Key_OpenBracket; break;
		case GLFW_KEY_RIGHT_BRACKET:	keyNum = Key_CloseBracket; break;

		case GLFW_KEY_PRINT_SCREEN:		keyNum = Key_Screenshot; break;
		case GLFW_KEY_HOME:				keyNum = Key_Home; break;
		case GLFW_KEY_END:				keyNum = Key_End; break;
		case GLFW_KEY_PAGE_DOWN:		keyNum = Key_PageDown; break;
		case GLFW_KEY_PAGE_UP:			keyNum = Key_PageUp; break;
		case GLFW_KEY_INSERT:			keyNum = Key_Insert; break;
		case GLFW_KEY_DELETE:			keyNum = Key_Delete; break;
		case GLFW_KEY_SCROLL_LOCK:		keyNum = Key_ScrollLock; break;
		case GLFW_KEY_CAPS_LOCK:		keyNum = Key_CapsLock;

		case GLFW_KEY_F1:				keyNum = Key_F1; break;
		case GLFW_KEY_F2:				keyNum = Key_F2; break;
		case GLFW_KEY_F3:				keyNum = Key_F3; break;
		case GLFW_KEY_F4:				keyNum = Key_F4; break;
		case GLFW_KEY_F5:				keyNum = Key_F5; break;
		case GLFW_KEY_F6:				keyNum = Key_F6; break;
		case GLFW_KEY_F7:				keyNum = Key_F7; break;
		case GLFW_KEY_F8:				keyNum = Key_F8; break;
		case GLFW_KEY_F9:				keyNum = Key_F9; break;
		case GLFW_KEY_F10:				keyNum = Key_F10; break;
		case GLFW_KEY_F11:				keyNum = Key_F11; break;
		case GLFW_KEY_F12:				keyNum = Key_F12; break;
		default:
			break;
		};
	}
	if ( key == GLFW_KEY_GRAVE_ACCENT ) {
		// console key, cannot be rebound
		keyNum = Key_Console;
	}

	return keyNum;
}
#endif