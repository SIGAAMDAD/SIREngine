#ifndef __TAB_SELECT_H__
#define __TAB_SELECT_H__

#pragma once

template<typename T>
struct TabData {
	CString name;
	T *pData;
	bool bOpened;
};

template<typename T>
class CTabSelect
{
public:	
	CTabSelect( void )
		: m_pCurrent( NULL )
	{ }
	~CTabSelect()
	{ }

	inline void AddItem( const TabData<T>& item )
	{ m_Items.emplace_back( item ); }

	inline TabData<T> *GetCurrent( void )
	{ return m_pCurrent; }
	inline bool Draw( void ) 
	{
		if ( m_Items.empty() ) {
			ImGui::TextUnformatted( "NONE OPENED" );
			return false;
		}
		if ( ImGui::BeginTabBar( SIRENGINE_TEMP_VSTRING( "##CTabSelect0x%lx", (uintptr_t)this ) ) ) {
			for ( auto& it : m_Items ) {
				if ( ImGui::BeginTabItem( it.name.c_str(), &it.bOpened ) ) {
					m_pCurrent = &it;
					ImGui::EndTabItem();
				}

				if ( !it.bOpened ) {
					TabData<T> *Old = m_pCurrent;
					if ( m_pCurrent == &it ) {
						if ( m_pCurrent == m_Items.begin() && m_Items.size() > 1 ) {
							m_pCurrent++;
						} else if ( m_pCurrent == m_Items.end() - 1 && m_Items.size() > 1 ) {
							m_pCurrent--;
						} else if ( m_Items.size() == 1 ) {
							m_pCurrent = NULL;
						}
					}
					m_Items.erase( Old );
				}
			}
			ImGui::EndTabBar();
		}

		return true;
	}

private:
	CVector<TabData<T>> m_Items;
	TabData<T> *m_pCurrent;
};

#endif