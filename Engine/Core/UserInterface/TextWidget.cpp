#include "TextWidget.h"
#include <ft2build.h>
#include FT_FREETYPE_H

using namespace GUILib;

CTextWidget::CTextWidget( const CString& id, const CString& text )
	: IWidgetBase( id ), m_TextBuffer( eastl::move( text ) )
{
}