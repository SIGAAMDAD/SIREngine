#include "SGame/InfoSystem/InfoDataManager.as"
#include "SGame/EntitySystem.as"

namespace TheNomad::SGame::InfoSystem {
    class MobInfo : InfoLoader {
		MobInfo() {
		}
		
		bool Load( json@ json ) {
			string armor;
			string str;
			array<json@> values;
			uint i;
			
			if ( !json.get( "Name", name ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'Name'\n" );
				return false;
			}
			if ( !json.get( "Type", str ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'Type'\n" );
				return false;
			} else {
				if ( !InfoManager.GetMobTypes().TryGetValue( str, this.type ) ) {
					GameError( "invalid mob info, Type \"" + str + "\" wasn't found" );
				}
			}
			if ( !json.get( "Health", health ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'Health'\n" );
				return false;
			}
			if ( !json.get( "ArmorType", armor ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'ArmorType'\n" );
				return false;
			}
			if ( !json.get( "Width", width ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'Width'\n" );
				return false;
			}
			if ( !json.get( "Height", height ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'Height'\n" );
				return false;
			}
			if ( !json.get( "SoundTolerance", soundTolerance ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'SoundTolerance'\n" );
				return false;
			}
		//	if ( !json.get( "SmellTolerance", smellTolerance ) ) {
		//		ConsoleWarning( "invalid mob info, missing variable 'SmellTolerance'\n" );
		//		return false;
		//	}
			if ( !json.get( "SightRadius", sightRadius ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'SightRadius'\n" );
				return false;
			}
			if ( !json.get( "SightRange", sightRange ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'SightRange'\n" );
				return false;
			}
			if ( !json.get( "DetectionRangeX", detectionRangeX ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'DetectionRangeX'\n" );
				return false;
			}
			if ( !json.get( "DetectionRangeY", detectionRangeY ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'DetectionRangeY'\n" );
				return false;
			}
			if ( !json.get( "WaitTics", waitTics ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'WaitTics'\n" );
				return false;
			}
			if ( !json.get( "SpriteOffsetX", spriteOffsetX ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'SpriteOffsetX'\n" );
				return false;
			}
			if ( !json.get( "SpriteOffsetY", spriteOffsetY ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'SpriteOffsetY'\n" );
				return false;
			}
			if ( !json.get( "Sprite", str ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'Sprite'\n" );
				return false;
			} else {
				hShader = Engine::Renderer::RegisterShader( str );
			}
			if ( !json.get( "WakeupSfx", str ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'WakeupSfx'\n" );
				return false;
			} else {
				wakeupSfx.Set( str );
			}
			if ( !json.get( "MoveSfx", str ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'MoveSfx'\n" );
				return false;
			} else {
				moveSfx.Set( str );
			}
			if ( !json.get( "PainSfx", str ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'PainSfx'\n" );
				return false;
			} else {
				painSfx.Set( str );
			}
			if ( !json.get( "DieSfx", str ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'DieSfx'\n" );
				return false;
			} else {
				dieSfx.Set( str );
			}

			if ( !json.get( "Flags", str ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'Flags'\n" );
				return false;
			}
			
			array<string>@ flagValues = TheNomad::Util::ParseCSV( str );
			
			ConsolePrint( "Processing MobFlags for '" + name + "'...\n" );
			for ( i = 0; i < MobFlagStrings.Count(); i++ ) {
				for ( uint a = 0; a < flagValues.Count(); a++ ) {
					if ( Util::StrICmp( flagValues[a], MobFlagStrings[i] ) == 0 ) {
						flags = EntityFlags( uint( flags ) | MobFlagBits[i] );
					}
				}
			}
			
			if ( !json.get( "Speed", values ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'Speed'\n" );
				return false;
			}
			
			if ( values.Count() != 3 ) {
				ConsoleWarning( "invalid mob info, Speed value List is not exactly 3 values.\n" );
				return false;
			}
			for ( i = 0; i < values.Count(); i++ ) {
				json data = values[i];
			}
			
			if ( !json.get( "AttackData", values ) ) {
				ConsoleWarning( "invalid mob info, missing variable 'AttackData'\n" );
				return false;
			}
			if ( values.Count() < 1 ) {
				ConsoleWarning( "mob info has no attack data.\n" );
				return false;
			}
			ConsolePrint( "Processing AttackData for MobInfo '" + name + "'...\n" );
			for ( i = 0; i < values.Count(); i++ ) {
				AttackInfo@ atk = AttackInfo();
				if ( !atk.Load( @values[i] ) ) {
					ConsoleWarning( "failed to load attack info.\n" );
					return false;
				}
				attacks.Add( @atk );
			}
			
			for ( i = 0; i < ArmorTypeStrings.Count(); i++ ) {
				if ( Util::StrICmp( armor, ArmorTypeStrings[i] ) == 0 ) {
					armorType = ArmorType( i );
					break;
				}
			}
			if ( i == ArmorTypeStrings.Count() ) {
				ConsoleWarning( "invalid mob info, ArmorType value '" + armor + "' not recognized.\n" );
				return false;
			}

			return true;
		}
		
		string name;
		float health = 0.0f;
		uint type = 0;
		ArmorType armorType = ArmorType::None;
		float width = 0.0f;
		float height = 0.0f;
		vec3 speed = vec3( 0.0f );
		float soundTolerance = 0.0f;
		float smellTolerance = 0.0f;
		float sightRadius = 0.0f;
		int sightRange = 0;
		int detectionRangeX = 0;
		int detectionRangeY = 0;
		MobFlags mobFlags = MobFlags::None;
		uint waitTics = 0; // the duration until the mob has to rethink again
		EntityFlags flags = EntityFlags::None;
		uint spriteOffsetX = 0;
		uint spriteOffsetY = 0;
		int hShader = 0;
		array<AttackInfo@> attacks;
		TheNomad::Engine::SoundSystem::SoundEffect wakeupSfx;
		TheNomad::Engine::SoundSystem::SoundEffect moveSfx;
		TheNomad::Engine::SoundSystem::SoundEffect painSfx;
		TheNomad::Engine::SoundSystem::SoundEffect dieSfx;
	};
};