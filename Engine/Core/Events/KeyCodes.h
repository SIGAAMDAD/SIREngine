#ifndef __SIRENGINE_KEYCODES_H__
#define __SIRENGINE_KEYCODES_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/Compiler.h>

typedef uint32_t KeyNum_t;

// keys should be passed as lowercased ASCII
enum : uint32_t {
	Key_A = 'a',
	Key_B = 'b',
	Key_C = 'c',
	Key_D = 'd',
	Key_E = 'e',
	Key_F = 'f',
	Key_G = 'g',
	Key_H = 'h',
	Key_I = 'i',
	Key_J = 'j',
	Key_K = 'k',
	Key_L = 'l',
	Key_M = 'm',
	Key_N = 'n',
	Key_O = 'o',
	Key_P = 'p',
	Key_Q = 'q',
	Key_R = 'r',
	Key_S = 's',
	Key_T = 't',
	Key_U = 'u',
	Key_V = 'v',
	Key_W = 'w',
	Key_X = 'x',
	Key_Y = 'y',
	Key_Z = 'z',

	Key_Tab = '\t',
	Key_Backspace = '\b',
	Key_Space = ' ',
	Key_Enter = '\r',

	Key_Quote = '\'',
	Key_Asterisk = '*',
	Key_Plus = '+',
	Key_Comma = ',',
	Key_Minus = '-',
	Key_Period = '.',
	Key_Slash = '/',
	Key_SemiColon = ';',
	Key_Equal = '=',
	Key_OpenBracket = '[',
	Key_BackSlash = '\\',
	Key_CloseBracket = ']',
	Key_Underscore = '_',

	Key_0 = '0',
	Key_1 = '1',
	Key_2 = '2',
	Key_3 = '3',
	Key_4 = '4',
	Key_5 = '5',
	Key_6 = '6',
	Key_7 = '7',
	Key_8 = '8',
	Key_9 = '9',

	Key_UpArrow,
	Key_DownArrow,
	Key_LeftArrow,
	Key_RightArrow,

	Key_Ctrl,
	Key_Shift,
	Key_Alt,
	Key_Insert,
	Key_Delete,
	Key_PageDown,
	Key_PageUp,
	Key_Home,
	Key_End,

	Key_MouseLeft,		// MOUSE_BUTTON_1
	Key_MouseMiddle,	// MOUSE_BUTTON_2
	Key_MouseRight,		// MOUSE_BUTTON_3
	Key_MouseButton4,
	Key_MouseButton5,
	Key_MouseWheelDown,
	Key_MouseWheelUp,

	Key_F1,
	Key_F2,
	Key_F3,
	Key_F4,
	Key_F5,
	Key_F6,
	Key_F7,
	Key_F8,
	Key_F9,
	Key_F10,
	Key_F11,
	Key_F12,

	Key_KP_0,
	Key_KP_1,
	Key_KP_2,
	Key_KP_3,
	Key_KP_4,
	Key_KP_5,
	Key_KP_6,
	Key_KP_7,
	Key_KP_8,
	Key_KP_9,

	Key_KP_Home,
	Key_KP_End,
	Key_KP_PageUp,
	Key_KP_PageDown,
	Key_KP_Enter,
	Key_KP_Insert,
	Key_KP_Delete,

	Key_Break,
	Key_ScrollLock,
	Key_Screenshot,
	Key_CapsLock,

	Key_Console,

	NumKeyIds
};

enum : uint32_t {
	GamePad_Button_Left,
	GamePad_Button_Right,
	GamePad_Button_Up,
	GamePad_Button_Down,

	GamePad_DPad_Left,
	GamePad_DPad_Right,
	GamePad_DPad_Up,
	GamePad_DPad_Down,

	GamePad_LeftStick_Click,
	GamePad_RightStick_Click,

	GamePad_LeftTrigger,
	GamePad_RightTrigger,

	GamePad_LeftButton,
	GamePad_RightButton,

	GamePad_Back,
	GamePad_Guide,
	GamePad_Start,

	GamePad_LeftStick_Left,
	GamePad_LeftStick_Right,
	GamePad_LeftStick_Up,
	GamePad_LeftStick_Down,

	GamePad_RightStick_Left,
	GamePad_RightStick_Right,
	GamePad_RightStick_Up,
	GamePad_RightStick_Down,

	// xbox names
	GamePad_X = GamePad_Button_Left,
	GamePad_B = GamePad_Button_Right,
	GamePad_Y = GamePad_Button_Up,
	GamePad_A = GamePad_Button_Down,

	// playstation names
	GamePad_Square = GamePad_Button_Left,
	GamePad_Circle = GamePad_Button_Right,
	GamePad_Triangle = GamePad_Button_Up,
	GamePad_Cross = GamePad_Button_Down,
};


#endif