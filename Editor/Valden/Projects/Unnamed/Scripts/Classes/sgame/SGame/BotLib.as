#include "SGame/MobObject.as"

// if you want custom mob behaviour, put it in here

namespace TheNomad::SGame {
	import vec3 Mob_GetOrigin( uint entityNumber ) from "nomadmain";
	import float Mob_GetHealth( uint entityNumber ) from "nomadmain";
	import uint Mob_GetStateId( uint entityNumber ) from "nomadmain";
	
	import void Mob_SetOrigin( uint entityNumber, vec3 origin ) from "nomadmain";
	import void Mob_SetHealth( uint entityNumber, float health ) from "nomadmain";
	import void Mob_SetStateId( uint entityNumber, uint stateId ) from "nomadmain";
	
	funcdef void Think( uint entityNumber );
	
	class StackMob {
		StackMob( uint entityNumber ) {
			this.entityNumber = entityNumber;
			origin = Mob_GetOrigin( entityNumber );
			stateId = Mob_GetStateId( entityNumber );
			health = Mob_GetHealth( entityNumber );
		}
		StackMob() {
		}
		~StackMob() {
			Mob_SetOrigin( entityNumber, origin );
			Mob_SetStateId( entityNumber, stateId );
			Mob_SetHealth( entityNumber, health );
		}
		
		vec3 origin = vec3( 0.0f );
		uint entityNumber = 0;
		uint stateId = 0;
		float health = 0.0f;
	};
	
	void Grunt_FightThink( uint entityNumber ) {
		StackMob mob( entityNumber );
		
		
	}
	void Grunt_IdThink( uint entityNumber ) {
		StackMob mob( entityNumber );
		
		
	}
	
	const Think[] s_Thinkers() = {
		Grunt_IdleThink
	};
	
	//
	// GetMobFuncIndexes: because of shared ode restrictions, we can't pass pointers
	// between modules, so instead we'll send an array of indexes to function pointers
	// from this module. It'll be just as fast, but slightly less efficient
	//
	void GetMobFuncIndexes( uint mobType, array<uint>@ indexes ) {
		uint index;
		
		index = mobType;
		
		for ( uint i = index; i < 
	}
	
	void Mob_Think()
};
