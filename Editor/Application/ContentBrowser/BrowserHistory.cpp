#include "BrowserHistory.h"

namespace Valden::ContentBrowser {

bool CBrowserHistory::GoBack( void )
{
	if ( CanGoBack() ) {
		--m_nCurrentHistoryIndex;
		return true;
	}
	return false;
}

bool CBrowserHistory::GoForward( void )
{
	if ( CanGoForward() ) {
		++m_nCurrentHistoryIndex;
		return true;
	}
	return false;
}

bool CBrowserHistory::CanGoBack( void ) const
{
	return m_HistoryData.size() > 0 && m_nCurrentHistoryIndex > 0;
}

bool CBrowserHistory::CanGoForward( void ) const
{
	return m_HistoryData.size() > 0 && m_nCurrentHistoryIndex < m_HistoryData.size() - 1;
}

FileView_t *CBrowserHistory::GetBack( void ) const
{
	if ( CanGoBack() ) {
		return m_HistoryData[ m_nCurrentHistoryIndex - 1 ];
	}
	return NULL;
}

FileView_t *CBrowserHistory::GetForward( void ) const
{
	if ( CanGoForward() ) {
		return m_HistoryData[ m_nCurrentHistoryIndex + 1 ];
	}
	return NULL;
}

void CBrowserHistory::AddHistoryData( FileView_t *dir )
{
	if ( m_HistoryData.size() == 0 ) {
		// added at beginning
		m_HistoryData.emplace_back( dir );
		m_nCurrentHistoryIndex = 0;
	}
	else if ( m_nCurrentHistoryIndex == m_HistoryData.size() - 1 ) {
		// added at end
		if ( m_HistoryData.size() == 256 ) {
			// if max history has been reached, pop
			// the oldest element
			m_HistoryData.erase( m_HistoryData.begin(), m_HistoryData.begin() + 1 );
		}
		m_HistoryData.emplace_back( dir );
		m_nCurrentHistoryIndex = m_HistoryData.size() - 1;
	}
	else {
		// history added somewhere in the middle
		// clear out everything after the current index
		m_HistoryData.erase( m_HistoryData.begin() + ( m_nCurrentHistoryIndex + 1 ), m_HistoryData.end() );
		m_HistoryData.emplace_back( dir );
		m_nCurrentHistoryIndex = m_HistoryData.size() - 1;
	}
}

};