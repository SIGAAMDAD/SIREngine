#include "Engine/Constants.as"

// essentially assert() but in angelscript
void Assert( const bool condition, const string& in msg ) {
	if ( !condition ) {
		GameError( "Assertion failure: " + msg );
	}
}

namespace TheNomad::Util {
	json@ LoadJSonFile( const string& in fileName ) {
		json@ data;

		@data = json();
		if ( !data.ParseFile( fileName ) ) {
			ConsoleWarning( "failed to load json file '" + fileName + "'\n" );
			return null;
		}

		return @data;
	}

	float CubicLerp( float a0, float a1, float w ) {
		if ( w < 0.0f ) {
			return a0;
		}
		if ( w > 1.0f ) {
			return a1;
		}
		return ( a1 - a0 ) * w+ a0;
	}

	float SmoothstepLerp( float a0, float a1, float w ) {
		if ( w < 0.0f ) {
			return a0;
		}
		if ( w > 1.0f ) {
			return a1;
		}
		return ( a1 - a0 ) * ( 3.0f - w * 2.0f ) * w * w + a0;
	}

	//
	// I did not write this, it came from psuedo code on wikipedia
	//
	vec2 randomGradient( int x, int y ) {
		// No precomupted gradients mean this works for any number of grid coordinates
		const uint w = 8 * TheNomad::Engine::Constants::SIZEOF_UINT;
		const uint s = w / 2; // rotation width
		uint a = uint( x );
		uint b = uint( y );
		float random;

		a *= 3284157443; b ^= a << s | a >> w - s;
		b *= 1911520717; a ^= a << s | b >> w - s;
		a *= 2048417325;

		random = a * ( M_PI / ~( ~0 >> 1 ) ); // in [0, 2*PI]

		return vec2( cos( random ), sin( random ) );
	}

	float dotGridGradient( float ix, float iy, float x, float y ) {
		const vec2 gradient = randomGradient( int( ix ), int( iy ) );

		const float dx = x - float( ix );
		const float dy = y - float( iy );

		return ( dx * gradient.x + dy * gradient.y );
	}

	float PerlinNoise( float x, float y ) {
		int x0 = int( floor( x ) );
		int x1 = x0 + 1;
		int y0 = int( floor( y ) );
		int y1 = y0 + 1;

		float sx = x - float( x0 );
		float sy = y - float( y0 );
		
		float n0, n1, ix0, ix1;
		
		n0 = dotGridGradient( x0, y0, x, y );
		n1 = dotGridGradient( x1, y0, x, y );
		ix0 = CubicLerp( n0, n1, sx );

		n0 = dotGridGradient( x0, y1, x, y );
		n1 = dotGridGradient( x1, y1, x, y );
		ix1 = CubicLerp( n0, n1, sy );

		return CubicLerp( ix0, ix1, sy );
	}

	void HapticRumble( uint nPlayerIndex, float nStrength, uint nTime ) {
		if ( TheNomad::Engine::CvarVariableInteger( "in_mode" ) == 1 ) {
			TheNomad::Engine::CmdExecuteCommand( "in_haptic_rumble " + nPlayerIndex + " " + formatFloat( nStrength ) + " " + nTime +  "\n" );
		}
	}

	/*
	const byte[] locase = {
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
		0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
		0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
		0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
		0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
		0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
		0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
		0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
		0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
		0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
		0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
		0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
		0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
		0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
		0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
		0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
		0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
		0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
		0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
		0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
		0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
		0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
		0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
		0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
		0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
		0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
		0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
		0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
		0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
		0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
		0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
		0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
	};
	*/

	// ASCII lowcase conversion table with '\\' turned to '/' and '.' to '\0'
	const uint8[] hash_locase = {
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
		0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
		0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
		0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
		0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
		0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x00,0x2f,
		0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
		0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
		0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
		0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
		0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
		0x78,0x79,0x7a,0x5b,0x2f,0x5d,0x5e,0x5f,
		0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
		0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
		0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
		0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
		0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
		0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
		0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
		0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
		0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
		0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
		0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
		0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
		0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
		0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
		0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
		0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
		0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
		0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
		0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
		0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
	};

	uint64 GenerateHashValue( const string& in fname, const uint64 size ) {
		int s;
		uint64 hash;
		int c;

		s = 0;
		hash = 0;

		while ( ( c = hash_locase[uint8(fname[s++])] ) != '\0' ) {
			hash = hash * 101 + c;
		}

		hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) );
		hash &= ( size - 1 );

		return hash;
	}

	int HashString( const string& in str ) {
		int i, hash = 0;
		int index = 0;
		for ( i = 0; str[index] != '\0'; i++ ) {
			hash += ( str[index++] ) * ( i + 119 );
		}
		return hash;
	}

	int HashString( const string& in str, int length ) {
		int i, hash = 0;
		int index = 0;
		for ( i = 0; i < length; i++ ) {
			hash += ( str[index++] ) * ( i + 119 );
		}
		return hash;
	}

	int IHashString( const string& in str ) {
		int i, hash = 0;
		int index = 0;
		for( i = 0; str[index] != '\0'; i++ ) {
			hash += ToLower( str[index++] ) * ( i + 119 );
		}
		return hash;
	}

	int IHashString( const string& in str, int length ) {
		int i, hash = 0;
		int index = 0;
		for ( i = 0; i < length; i++ ) {
			hash += ToLower( str[index++] ) * ( i + 119 );
		}
		return hash;
	}

	int8 ToLower( int8 c ) {
		if ( IsUpper( c ) ) {
			c += ( 'a' - 'A' );
		}
		return c;
	}

	int8 ToUpper( int8 c ) {
		if ( IsLower( c ) ) {
			c -= ( 'a' - 'A' );
		}
		return c;
	}

	uint32 ColorAsUInt32( const vec4& in color ) {
		// convert color components to value between 0 and 255.
	    const uint32 r = 255 * uint32( color.r );
		const uint32 g = 255 * uint32( color.g );
	    const uint32 b = 255 * uint32( color.b );

    	// combine the color components in a single value of the form 0xAaBbGgRr
	    return 0xFF000000 | r | ( b << 16 ) | ( g << 8 );
	}

	array<string>@ ParseCSV( const string& in str ) {
	    array<string> values;
	    string data;

	    for ( uint i = 0; i < str.Length(); i++ ) {
	        switch ( str[i] ) {
	        case ',':
	            values.Add( data );
	            data = "";
	            i++;
	            break;
	        case ' ':
	            break; // ignore it
	        default:
	            data += str[i];
	            break;
	        };
	    }
	
	    return @values;
	}

	const vec4& StringToColor( const string& in color ) {
		if ( StrICmp( color, "black" ) == 0 ) {
			return colorBlack;
		} else if ( StrICmp( color, "red" ) == 0 ) {
			return colorRed;
		} else if ( StrICmp( color, "green" ) == 0 ) {
			return colorGreen;
		} else if ( StrICmp( color, "yellow" ) == 0 ) {
			return colorYellow;
		} else if ( StrICmp( color, "blue" ) == 0 ) {
			return colorBlue;
		} else if ( StrICmp( color, "cyan" ) == 0 ) {
			return colorCyan;
		} else if ( StrICmp( color, "magenta" ) == 0 ) {
			return colorMagenta;
		} else if ( StrICmp( color, "gold" ) == 0 ) {
			return colorGold;
		} else if ( StrICmp( color, "white" ) == 0 ) {
			return colorWhite;
		} else {
			ConsoleWarning( "StringToColor: invalid color string \"" + color + "\"\n" );
		}
		return colorWhite;
	}

	float Swap( float a, float b ) {
		float tmp = a;
		a = b;
		b = tmp;
		return tmp;
	}
	int Swap( int a, int b ) {
		int tmp = 0;
		a = b;
		b = tmp;
		return tmp;
	}
	uint Swap( uint a, uint b ) {
		uint tmp = 0;
		a = b;
		b = tmp;
		return tmp;
	}

	//
	// M_Random
	// Returns a 0-255 number
	//

	const uint8[] rndtable = {
		0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66 ,
	    74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36 ,
	    95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188 ,
	    52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224 ,
	    149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242 ,
	    145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0 ,
	    175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235 ,
	    25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113 ,
	    94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75 ,
	    136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196 ,
	    135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113 ,
	    80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241 ,
	    24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224 ,
	    145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95 ,
	    28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226 ,
	    71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36 ,
	    17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106 ,
	    197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136 ,
	    120, 163, 236, 249,

		0
	};

	int	rndindex = 0;
	int	prndindex = 0;

	// Which one is deterministic?
	int PRandom() {
	    prndindex = ( prndindex + 1 ) & 0xff;
	    return rndtable[ prndindex ];
	}

	int MRandom() {
	    rndindex = ( rndindex + 1 ) & 0xff;
	    return rndtable[ rndindex ];
	}

	void MClearRandom() {
	    rndindex = prndindex = 0;
	}

	int Clamp( int value, int min, int max ) {
		if ( value > max ) {
			return max;
		} else if ( value < min ) {
			return min;
		}
		return value;
	}
	
	uint Clamp( uint value, uint min, uint max ) {
		if ( value > max ) {
			return max;
		} else if ( value < min ) {
			return min;
		}
		return value;
	}

	float Clamp( float value, float min, float max ) {
		if ( value > max ) {
			return max;
		} else if ( value < min ) {
			return min;
		}
		return value;
	}

	int Hex( char c ) {
		if ( c >= '0' && c <= '9' ) {
			return c - '0';
		} else if ( c >= 'A' && c <= 'F' ) {
			return 10 + c - 'A';
		} else if ( c >= 'a' && c <= 'f' ) {
			return 10 + c - 'a';
		}

		return -1;
	}

	int HexStringToInt( const string& in str ) {
		if ( str.Length() < 1 ) {
			return -1;
		}

		// check for hex code
		if ( str[ 0 ] == '0' && str[ 1 ] == 'x' && str[ 2 ] != '\0' ) {
		    int i, digit, n = 0, len = str.Length();

			for ( i = 2; i < len; i++ ) {
				n *= 16;

				digit = Hex( str[ i ] );

				if ( digit < 0 ) {
					return -1;
				}

				n += digit;
			}

			return n;
		}

		return -1;
	}

	bool GetHashColor( const string& in str, vec3& out color ) {
		uint i, len;
		int[] hex( 6 );

		color[0] = color[1] = color[2] = 0;

		if ( str[1] != '#' ) {
			return false;
		}

		len = str.Length();
		if ( len <= 0 || len > 6 ) {
			return false;
		}

		for ( i = 1; i < len; i++ ) {
			hex[i] = Hex( str[i] );
			if ( hex[i] < 0 ) {
				return false;
			}
		}

		switch ( len ) {
		case 3: // #rgb
			color[0] = hex[0] << 4 | hex[0];
			color[1] = hex[1] << 4 | hex[1];
			color[2] = hex[2] << 4 | hex[2];
			break;
		case 6: // #rrggbb
			color[0] = hex[0] << 4 | hex[1];
			color[1] = hex[2] << 4 | hex[3];
			color[2] = hex[4] << 4 | hex[5];
			break;
		default: // unsupported format
			return false;
		};

		return true;
	}

	void CrossProduct( const vec3& in v1, const vec3& in v2, vec3& out cross ) {
		cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
		cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
		cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
	}

	float RadiusFromBounds( const TheNomad::GameSystem::BBox& in bounds ) {
		int		i;
		vec3	corner;

		// quiet down compiler
		float	a = 0.0f;
		float	b = 0.0f;

		for ( i = 0; i < 3; i++ ) {
			a = abs( bounds.m_Mins[i] );
			b = abs( bounds.m_Maxs[i] );
			corner[i] = a > b ? a : b;
		}

		return VectorLength( corner );
	}

	void VectorScale( const vec3& in src, float scale, vec3& out dst ) {
		dst[0] = src[0]*scale;
		dst[1] = src[1]*scale;
		dst[2] = src[2]*scale;
	}

	float DotProduct( const vec3& in a, const vec3& in b ) {
		return ( a[0]*b[0] + a[1]*b[1] + a[2]*b[2] );
	}

	float VectorLength( const vec3& in v ) {
		return sqrt( DotProduct( v, v ) );
	}

	float VectorNormalize( vec3& out v ) {
		float length, ilength;

		length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

		if ( length > 0.0f ) {
			ilength = 1.0f / float( sqrt( length ) );
			/* sqrt(length) = length * (1 / sqrt(length)) */
			length *= ilength;
			v[0] *= ilength;
			v[1] *= ilength;
			v[2] *= ilength;
		}
		
		return length;
	}


	int8 ClampChar( int8 i ) {
		if ( i < -128 ) {
			return -128;
		}
		if ( i > 127 ) {
			return 127;
		}
		return i;
	}

	int8 ClampCharMove( int8 i ) {
		if ( i < -127 ) {
			return -127;
		}
		if ( i > 127 ) {
			return 127;
		}
		return i;
	}

	int16 ClampShort( int16 i ) {
		if ( i < -32768 ) {
			return -32768;
		}
		if ( i > 0x7fff ) {
			return 0x7fff;
		}
		return i;
	}

	bool IsPrint( int c ) {
		if ( c >= 0x20 && c <= 0x7E ) {
			return true;
		}
		return false;
	}

	bool IsLower( int c ) {
		if ( c >= 'a' && c <= 'z' ) {
			return true;
		}
		return false;
	}

	bool IsUpper( int c ) {
		if ( c >= 'A' && c <= 'Z' ) {
			return true;
		}
		return false;
	}

	bool IsAlpha( int c ) {
		if ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ) {
			return true;
		}
		return false;
	}

	bool IsIntegral( float f ) {
		return ( int( f ) == f );
	}

	float Lerp( float a, float b, float f ) {
		return ( a * ( 1.0f - f ) ) + ( b * f );
	}

	float DEG2RAD( float x ) {
		return ( ( x * M_PI ) / 180.0F );
	}

	float RAD2DEG( float x ) {
		return ( ( x * 180.0f ) / M_PI );
	}

	//
	// Dir2Angle: returns absolute degrees
	//
	float Dir2Angle( TheNomad::GameSystem::DirType dir ) {
		switch ( dir ) {
		case TheNomad::GameSystem::DirType::North:
		case TheNomad::GameSystem::DirType::Inside:
			return 0.0f;
		case TheNomad::GameSystem::DirType::NorthEast:
			return 45.0f;
		case TheNomad::GameSystem::DirType::East:
			return 90.0f;
		case TheNomad::GameSystem::DirType::SouthEast:
			return 135.0f;
		case TheNomad::GameSystem::DirType::South:
			return 180.0f;
		case TheNomad::GameSystem::DirType::SouthWest:
			return 225.0f;
		case TheNomad::GameSystem::DirType::West:
			return 270.0f;
		case TheNomad::GameSystem::DirType::NorthWest:
			return 315.0f;
		default:
			GameError( "Dir2Angle: invalid dir " + formatUInt( dir ) );
		};
		return -1.0f;
	}

	//
	// Angle2Dir:
	//
	TheNomad::GameSystem::DirType Angle2Dir( float angle ) {
		if ( ( angle >= 337.5f && angle <= 360.0f ) || ( angle >= 0.0f && angle <= 22.5f ) ) {
			return TheNomad::GameSystem::DirType::North;
		} else if ( angle >= 22.5f && angle <= 67.5f ) {
			return TheNomad::GameSystem::DirType::NorthEast;
		} else if ( angle >= 67.5f && angle <= 112.5f ) {
			return TheNomad::GameSystem::DirType::East;
		} else if ( angle >= 112.5f && angle <= 157.5f ) {
			return TheNomad::GameSystem::DirType::SouthEast;
		} else if ( angle >= 157.5f && angle <= 202.5f ) {
			return TheNomad::GameSystem::DirType::South;
		} else if ( angle >= 202.5f && angle <= 247.5f ) {
			return TheNomad::GameSystem::DirType::SouthWest;
		} else if ( angle >= 247.5f && angle <= 292.5f ) {
			return TheNomad::GameSystem::DirType::West;
		} else if ( angle >= 292.5f && angle <= 337.5f ) {
			return TheNomad::GameSystem::DirType::NorthWest;
		} else {
			ConsoleWarning( "Angle2Dir: funny angle " + angle + "\n" );
		}
		return TheNomad::GameSystem::DirType::North;
	}
};