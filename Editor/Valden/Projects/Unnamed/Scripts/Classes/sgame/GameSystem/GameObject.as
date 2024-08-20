
namespace TheNomad::GameSystem {
    interface GameObject {
		void OnInit();
		void OnShutdown();

		void OnLevelStart();
		void OnLevelEnd();

		void OnLoad();
		void OnSave() const;

		void OnPlayerDeath( int nPlayerIndex );
		void OnCheckpointPassed( uint nCheckpointIndex );

		void OnRunTic();
		void OnRenderScene();

		const string& GetName() const;
	};
};