#ifndef __SIRENGINE_TEXTURE_ATLAS_H__
#define __SIRENGINE_TEXTURE_ATLAS_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/ResourceDef.h>
#include <Engine/Core/MathLib/Types.h>
#include <Engine/Util/CVector.h>

namespace SIREngine {
	typedef struct {
		CVec2 TexCoords;
		CIVec2 SpritePosition;
	} AtlasSprite_t;

	class CTextureAtlas : public CResourceDef
	{
	public:
		CTextureAtlas( void );
		virtual ~CTextureAtlas() override;


	private:
		CIVec2 m_Size;
		uint32_t m_nTextureCountX;
		uint32_t m_nTextureCountY;
		uint32_t m_nTextureWidth;
		uint32_t m_nTextureHeight;
		CVector<AtlasSprite_t> m_Sprites;
	};
};

#endif