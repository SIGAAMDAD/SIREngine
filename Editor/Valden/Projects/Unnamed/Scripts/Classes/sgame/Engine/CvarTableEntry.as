namespace TheNomad::Engine {
    class CvarTableEntry {
		CvarTableEntry() {
		}
		CvarTableEntry( ConVar@ cvar ) {
			@m_Handle = @cvar;
		}

		ConVar@ m_Handle = null;
		int m_nModificationCount = 0;
	};
};