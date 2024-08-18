#ifndef __GUILIB_WIDGET_H__
#define __GUILIB_WIDGET_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Util/CString.h>

namespace SIREngine::GUILib {
	class IWidgetBase
	{
	public:
		IWidgetBase( const CString& name );
		virtual ~IWidgetBase();

		virtual void Draw( void ) = 0;
	private:
		CString m_Name;
	};

	class CSliderWidget : public IWidgetBase
	{
	public:
		CSliderWidget( const CString& name );
	private:

	};
};

#endif