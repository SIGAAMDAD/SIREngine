#ifndef __GUILIB_TEXTWIDGET_H__
#define __GUILIB_TEXTWIDGET_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Widget.h"

namespace SIREngine::GUILib {
	class CTextWidget : public IWidgetBase
	{
	public:	
		CTextWidget( const CString& id, const CString& text );
		virtual ~CTextWidget() override;

		virtual void Draw( void ) override;
	private:
		CString m_TextBuffer;
	};
};

#endif