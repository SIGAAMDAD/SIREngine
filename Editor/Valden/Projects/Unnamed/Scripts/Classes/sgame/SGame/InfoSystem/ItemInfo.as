#include "SGame/InfoSystem/InfoDataManager.as"

namespace TheNomad::SGame::InfoSystem {
    class ItemInfo : InfoLoader {
		ItemInfo() {
		}
		
		bool Load( json@ json ) {
			string str;
			bool useSpriteSheet = false;

			if ( !json.get( "Name", name ) ) {
				ConsoleWarning( "invalid item info, missing variable 'Name'\n" );
				return false;
			}
			if ( !json.get( "Id", str ) ) {
				ConsoleWarning( "invalid item info, missing variable 'Id'\n" );
				return false;
			} else {
				if ( !InfoManager.GetItemTypes().TryGetValue( str, type ) ) {
					GameError( "invalid item info, Type \"" + str + "\" wasn't found" );
				}
			}
			if ( !json.get( "Effect", effect ) ) {
				ConsoleWarning( "invalid item info, missing variable 'Effect'\n" );
				return false;
			}
			json.get( "Cost", cost );
			if ( !json.get( "PickupSfx", str ) ) {
				ConsoleWarning( "invalid item info, missing variable 'PickupSfx'\n" );
				return false;
			} else {
				pickupSfx = TheNomad::Engine::ResourceCache.GetSfx( str );
			}
			if ( !json.get( "UseSfx", str ) ) {
				ConsoleWarning( "invalid item info, missing variable 'UseSfx'\n" );
				return false;
			} else {
				useSfx = TheNomad::Engine::ResourceCache.GetSfx( str );
			}
			if ( !json.get( "EquipSfx", str ) ) {
				ConsoleWarning( "invalid item info, missing variable 'EquipSfx'\n" );
				return false;
			} else {
				equipSfx = TheNomad::Engine::ResourceCache.GetSfx( str );
			}
			if ( !json.get( "Width", width ) ) {
				ConsoleWarning( "invalid item info, missing variable 'Width'\n" );
				return false;
			}
			if ( !json.get( "Height", height ) ) {
				ConsoleWarning( "invalid item info, missing variable 'Height'\n" );
				return false;
			}
			if ( !json.get( "Icon", str ) ) {
				ConsoleWarning( "invalid item info, missing variable 'Icon'\n" );
				return false;
			} else {
				iconShader = TheNomad::Engine::ResourceCache.GetShader( str );
			}

			TheNomad::GameSystem::GetString( name + "_DESC", description );

			ConsolePrint( "Loaded item info for '" + name + "'\n" );

			return true;
		}

		string name;
		string description;
		string effect;
		int iconShader = FS_INVALID_HANDLE;
		uint type = 0;
		int cost = 0;
		uint maxStackSize = 0;
		float width = 0.0f;
		float height = 0.0f;

		TheNomad::Engine::SoundSystem::SoundEffect pickupSfx;
		TheNomad::Engine::SoundSystem::SoundEffect useSfx;
		TheNomad::Engine::SoundSystem::SoundEffect equipSfx;
	};
};