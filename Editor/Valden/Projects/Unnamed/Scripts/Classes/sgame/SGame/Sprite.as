#include "SGame/SpriteSheet.as"

namespace TheNomad::SGame {
    //
	// DrawRect: quick and easy utility function
	//
	void DrawRect( const vec3& in origin, const vec2& in size, int hShader ) {
		const float scale = TheNomad::GameSystem::GameManager.GetUIScale();
		ImGui::Image( hShader, vec2( origin.x * scale, origin.y * scale ), vec2( size.x * scale, size.y * scale ) );
	}

	class Sprite {
		Sprite() {
		}
		Sprite( const ivec2& in offset, const vec2& in sheetSize, const vec2& in spriteSize, int hShader ) {
			Set( offset, sheetSize, spriteSize );
			m_hShader = hShader;
			m_SpriteSize = spriteSize;
		}

		Sprite& opAssign( const Sprite& in other ) {
			m_TexCoords0 = other.m_TexCoords0;
			m_TexCoords1 = other.m_TexCoords1;
			m_TexCoords2 = other.m_TexCoords2;
			m_TexCoords3 = other.m_TexCoords3;

			return this;
		}

		void Set( const ivec2& in offset, const vec2& in sheetSize, const vec2& in spriteSize ) {
			const vec2 min( ( ( float( offset.x ) + 1.0f ) * spriteSize.x ) / sheetSize.x,
				( ( float( offset.y ) + 1.0f ) * spriteSize.y ) / sheetSize.y );

			const vec2 max( ( float( offset.x ) * spriteSize.x ) / sheetSize.x,
				( float( offset.y ) * spriteSize.y ) / sheetSize.y );
			
			m_TexCoords0[0] = min[0];
    		m_TexCoords0[1] = max[1];

    		m_TexCoords1[0] = min[0];
    		m_TexCoords1[1] = min[1];

    		m_TexCoords2[0] = max[0];
    		m_TexCoords2[1] = min[1];
    
    		m_TexCoords3[0] = max[0];
    		m_TexCoords3[1] = max[1];

			DebugPrint( "Generated Sprite Coordinates: (offset)[ " + offset.x + ", " + offset.y + " ]\n" );
			DebugPrint( "TexCoords[0]: " + m_TexCoords0[0] + ", " + m_TexCoords0[1] + "\n" );
			DebugPrint( "TexCoords[1]: " + m_TexCoords1[0] + ", " + m_TexCoords1[1] + "\n" );
			DebugPrint( "TexCoords[2]: " + m_TexCoords2[0] + ", " + m_TexCoords2[1] + "\n" );
			DebugPrint( "TexCoords[3]: " + m_TexCoords3[0] + ", " + m_TexCoords3[1] + "\n" );
		}

		void Draw( const vec3& in origin, float size = 1.0f ) const {
			TheNomad::Engine::Renderer::DrawImage( origin.x, origin.y, 64, 64,
//				0, 0, 1, 1, m_hShader );
				m_TexCoords0[0], m_TexCoords0[1], m_TexCoords3[0], m_TexCoords3[1], m_hShader );
		}

		const vec2& opIndex( uint nIndex ) const {
			switch ( nIndex ) {
			case 0: return m_TexCoords0;
			case 1: return m_TexCoords1;
			case 2: return m_TexCoords2;
			case 3: return m_TexCoords3;
			default:
				GameError( "out of range index" );
			};
			return m_TexCoords0;
		}
		vec2& opIndex( uint nIndex ) {
			switch ( nIndex ) {
			case 0: return m_TexCoords0;
			case 1: return m_TexCoords1;
			case 2: return m_TexCoords2;
			case 3: return m_TexCoords3;
			default:
				GameError( "out of range index" );
			};
			return m_TexCoords0;
		}

		int GetShader() const {
			return m_hShader;
		}

		// using an array here would be very innefficient
		private vec2 m_TexCoords0;
		private vec2 m_TexCoords1;
		private vec2 m_TexCoords2;
		private vec2 m_TexCoords3;

		private vec2 m_SpriteSize;
		private int m_hShader = FS_INVALID_HANDLE;
	};
};