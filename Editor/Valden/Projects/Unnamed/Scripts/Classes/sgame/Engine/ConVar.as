#include "main.as"

namespace TheNomad {
	//
	// ConVar: an implementation of the vmCvar_t, but to make it easier to manage,
	// its been put here
	// DO NOT MODIFY
	//
	class ConVar {
		ConVar() {
		}
		
		void Register( const string& in name, const string& in value, uint flags, bool bTrackChanges ) {
			m_Name = name;
			m_Value = value;
			m_Flags = flags;
			m_bTrackChanges = bTrackChanges;
			Engine::CvarRegister( name, value, flags, m_IntValue, m_FloatValue, m_nModificationCount, m_nCvarHandle );
		}
		
		const string& GetName() const {
			return m_Name;
		}
		const string& GetValue() const {
			return m_Value;
		}
		int32 GetInt() const {
			return m_IntValue;
		}
		float GetFloat() const {
			return m_FloatValue;
		}
		uint GetFlags() const {
			return m_Flags;
		}
		int GetModificationCount() const {
			return m_nModificationCount;
		}
		bool Track() const {
			return m_bTrackChanges;
		}
		
		void Set( const string& in value ) {
			Engine::CvarSet( m_Name, value );
		}
		void Update() {
			int64 intValue;
			if ( m_nCvarHandle == FS_INVALID_HANDLE ) {
				Engine::CvarRegister( m_Name, m_Value, m_Flags, intValue, m_FloatValue, m_nModificationCount, m_nCvarHandle );
			}
			Engine::CvarUpdate( m_Value, intValue, m_FloatValue, m_nModificationCount, m_nCvarHandle );
			m_IntValue = intValue;
		}
		
		private string m_Name = "";
		private string m_Value = "";
		private int m_IntValue = 0;
		private float m_FloatValue = 0.0f;
		private uint m_Flags = 0;
		private int m_nModificationCount = 0;
		private int m_nCvarHandle = FS_INVALID_HANDLE;
		private bool m_bTrackChanges = false;
	};
};