#ifndef __VALDEN_FILEVIEW_H__
#define __VALDEN_FILEVIEW_H__

#pragma once

#include <Engine/Core/UserInterface/GUILib.h>
#include <Engine/Core/UserInterface/Widget.h>
#include <Engine/Core/FileSystem/FileList.h>

namespace Valden {
	class CFileView : public GUILib::IWidgetBase
	{
	public:
		CFileView( void );
		virtual ~CFileView() override;

		virtual void Draw( void ) override;
	private:
		SIREngine::FileSystem::CFileList *m_pFileList;
	};
};

#endif