#ifndef __VALDEN_CONTENTBROWSER_HISTORY_H__
#define __VALDEN_CONTENTBROWSER_HISTORY_H__

#pragma once

#include "../Project/ProjectManager.h"
#include "FileView.h"

namespace Valden::ContentBrowser {
	class CBrowserHistory
	{
	public:
		CBrowserHistory( void )
		{ }
		~CBrowserHistory()
		{ }

		bool CanGoBack( void ) const;
		bool CanGoForward( void ) const;

		bool GoForward( void );
		bool GoBack( void );

		void AddHistoryData( FileView_t *dir );

		FileView_t *GetBack( void ) const;
		FileView_t *GetForward( void ) const;
	private:
		CVector<FileView_t *> m_HistoryData;
		uint64_t m_nCurrentHistoryIndex;
	};
};

#endif