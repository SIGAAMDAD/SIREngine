#ifndef __VALDEN_SCRIPT_TAB_SELECT_H__
#define __VALDEN_SCRIPT_TAB_SELECT_H__

#pragma once

#include <EASTL/vector_map.h>

namespace Valden {
	class CScriptObject;

	struct ScriptTabData {
		char szName[ MAX_RESOURCE_PATH ];
		char szDisplayName[ MAX_RESOURCE_PATH ]; // just szName but with an extra "*" at the end if its been modified
		CScriptObject *pData;
		bool bOpened;
	};

	class CScriptTabSelect
	{
	public:
		using ScriptTabList = eastl::vector_map<CString, ScriptTabData, eastl::less<CString>, MemoryAllocator<char>>;

		CScriptTabSelect( void )
			: m_pCurrent( NULL )
		{ }
		~CScriptTabSelect()
		{ }

		inline void AddItem( const ScriptTabData& item )
		{
			if ( m_Items.find( item.szName ) != m_Items.end() ) {
				return; // already in there
			}
			m_Items[ item.szName ] = item;
		}
		inline ScriptTabList& GetItems( void )
		{ return m_Items; }

		inline ScriptTabData *GetCurrent( void )
		{ return m_pCurrent ? &m_pCurrent->second : NULL; }
		inline bool Draw( void ) 
		{
			if ( m_Items.empty() ) {
				ImGui::TextUnformatted( "NONE OPENED" );
				return false;
			}
			if ( ImGui::BeginTabBar( SIRENGINE_TEMP_VSTRING( "##CTabSelect0x%lx", (uintptr_t)this ) ) ) {
				for ( auto& it : m_Items ) {
					if ( ImGui::BeginTabItem( it.second.szDisplayName, &it.second.bOpened ) ) {
						m_pCurrent = &it;
						ImGui::EndTabItem();
					}

					if ( !it.second.bOpened ) {
						auto Old = m_pCurrent;
						if ( m_pCurrent == &it ) {
							if ( m_Items.size() == 1 ) {
								m_pCurrent = NULL;
								m_Items.clear();
							} else {
								if ( m_pCurrent == m_Items.begin() ) {
									m_pCurrent++;
								} else if ( m_pCurrent == m_Items.end() - 1 ) {
									m_pCurrent--;
								}
								m_Items.erase( Old );
							}
						}
					}
				}
				ImGui::EndTabBar();
			}

			return true;
		}

	private:
		ScriptTabList m_Items;
		ScriptTabList::iterator m_pCurrent;
	};
};

#endif