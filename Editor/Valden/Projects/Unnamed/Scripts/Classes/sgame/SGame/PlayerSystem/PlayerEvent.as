namespace TheNomad::SGame {
    interface PlayerEvent {
		void OnRunTic( PlayrObject@ ent );
		void Activate( PlayrObject@ ent );
		bool Load( json@ json );
		bool IsActive() const;
	};
};