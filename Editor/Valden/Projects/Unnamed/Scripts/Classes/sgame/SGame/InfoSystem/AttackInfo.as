#include "SGame/InfoSystem/InfoDataManager.as"

namespace TheNomad::SGame::InfoSystem {
   	class AttackInfo : InfoLoader {
		AttackInfo() {
		}
		
		bool Load( json@ json ) {
			string methodStr;
			string typeStr;
			string shader;
			string bullet;
			uint i;

			if ( !json.get( "Id", id ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'Id'\n" );
				return false;
			}
			json.get( "Effect", effect );
			if ( !json.get( "Damage", damage ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'Damage'\n" );
				return false;
			}
			if ( !json.get( "Range", range ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'Range'\n" );
				return false;
			}
			if ( !json.get( "Cooldown", cooldown ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'Cooldown'\n" );
				return false;
			}
			if ( !json.get( "Duration", duration ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'Duration'\n" );
				return false;
			}
			if ( !json.get( "Method", methodStr ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'Method'\n" );
				return false;
			}
			if ( !json.get( "Type", typeStr ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'Type'\n" );
				return false;
			}
			if ( !json.get( "CanParry", canParry ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'CanParry'\n" );
				return false;
			}
			if ( !json.get( "Sprite", shader ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'Sprite'\n" );
				return false;
			} else {
				hShader = TheNomad::Engine::Renderer::RegisterShader( shader );
			}
			if ( !json.get( "SpriteOffsetX", spriteOffsetX ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'SpriteOffsetX'\n" );
				return false;
			}
			if ( !json.get( "SpriteOffsetY", spriteOffsetY ) ) {
				ConsoleWarning( "invalid mob attack info, missing variable 'SpriteOffsetY'\n" );
				return false;
			}
			
			GameSystem::GetString( id + "_DESC", description );

			for ( i = 0; i < AttackMethodStrings.Count(); i++ ) {
				if ( Util::StrICmp( AttackMethodStrings[i], methodStr ) == 0 ) {
					attackMethod = AttackMethod( i );
					break;
				}
			}
			if ( i == AttackMethodStrings.Count() ) {
				ConsoleWarning( "invalid attack info, AttackMethod '" + methodStr + "' isn't recognized.\n" );
				return false;
			}

			for ( i = 0; i < AttackTypeStrings.Count(); i++ ) {
				if ( Util::StrICmp( AttackTypeStrings[i], typeStr ) == 0 ) {
					attackType = AttackType( i );
					break;
				}
			}
			if ( i == AttackTypeStrings.Count() ) {
				ConsoleWarning( "invalid attack info, AttackType '" + typeStr + "' isn't recognized.\n" );
				return false;
			}

			switch ( attackMethod ) {
			case AttackMethod::Hitscan: {
				if ( !json.get( "BulletType", bullet ) ) {
					ConsoleWarning( "invalid attack info, marked as Hitscan but missing variable 'BulletType'\n" );
					return false;
				}
				if ( !json.get( "BulletCount", bulletCount ) ) {
					ConsoleWarning( "invalid attack info, marked as Hitscan but missing variable 'BulletCount'\n" );
					return false;
				}
				@bulletType = InfoManager.GetAmmoInfo( bullet );
				if ( @bulletType is null ) {
					ConsoleWarning( "invalid attack info, marked as Hitscan but BulletType is invalid\n" );
					return false;
				}
				break; }
			default:
				break;
			};

			ConsolePrint( "Loaded mob attack info '" + id + "'\n" );
			
			valid = true;

			return true;
		}
		
		string id;
		string effect;
		string description;
		float damage = 0.0f;
		float range = 0.0f;
		AmmoInfo@ bulletType = null;
		uint bulletCount = 0;
		uint cooldown = 0;
		uint duration = 0;
		AttackMethod attackMethod = AttackMethod::Hitscan;
		AttackType attackType = AttackType::Melee;
		bool canParry = true;
		bool valid = false;
		int hShader = FS_INVALID_HANDLE;
		uint spriteOffsetX = 0;
		uint spriteOffsetY = 0;
		TheNomad::Engine::SoundSystem::SoundEffect sound;

		// projectile specific vars
		uint projectileEntityType = 0;
		float projectileWidth = 0.0f;
		float projectileHeight = 0.0f;
	};
};