#include "TextWidget.h"
#include <ft2build.h>
#include FT_FREETYPE_H

namespace SIREngine::GUILib {

static void InitFreetype2()
{
	FT_InitFreeType();
}

CTextWidget::CTextWidget( const CString& id, const CString& text )
	: IWidgetBase( id ), m_TextBuffer( eastl::move( text ) )
{
}

};