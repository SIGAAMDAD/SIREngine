#include "SGame/InfoSystem/InfoDataManager.as"

namespace TheNomad::SGame::InfoSystem {
    class WeaponInfo : ItemInfo {
		WeaponInfo() {
		}
		
		bool Load( json@ json ) {
			uint i;
			array<json@> props;
			string ammo;
			string str;
			string type;
			bool useSpriteSheet = false;

			if ( !json.get( "Name", name ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'Name'\n" );
				return false;
			}
			if ( !json.get( "Id", type ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'Id'\n" );
				return false;
			} else {
				if ( !InfoManager.GetWeaponTypes().TryGetValue( type, this.type ) ) {
					GameError( "invalid weapon info, Type \"" + type + "\" wasn't found" );
				}
			}
			if ( !json.get( "Effect", effect ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'Effect'\n" );
				return false;
			}
			json.get( "Cost", cost );
			if ( !json.get( "PickupSfx", str ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'PickupSfx'\n" );
				return false;
			} else {
				pickupSfx = TheNomad::Engine::ResourceCache.GetSfx( str );
			}
			if ( !json.get( "UseSfx", str ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'UseSfx'\n" );
				return false;
			} else {
				useSfx = TheNomad::Engine::ResourceCache.GetSfx( str );
			}
			if ( !json.get( "EquipSfx", str ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'EquipSfx'\n" );
				return false;
			} else {
				equipSfx = TheNomad::Engine::ResourceCache.GetSfx( str );
			}
			if ( !json.get( "Width", width ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'Width'\n" );
				return false;
			}
			if ( !json.get( "Height", height ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'Height'\n" );
				return false;
			}
			if ( !json.get( "Icon", str ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'Icon'\n" );
				return false;
			} else {
				iconShader = TheNomad::Engine::ResourceCache.GetShader( str );
			}

			TheNomad::GameSystem::GetString( name + "_DESC", description );

			if ( !json.get( "MagSize", magSize ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'MagSize'\n" );
				return false;
			}
			if ( !json.get( "Damage", damage ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'Damage'\n" );
				return false;
			}
			if ( !json.get( "Range", range ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'Range'\n" );
				return false;
			}
			if ( !json.get( "WeaponProperties", props ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'WeaponProperties'\n" );
				return false;
			}
			if ( !json.get( "AmmoType", ammo ) ) {
				ConsoleWarning( "invalid weapon info, missing variable 'AmmoType'\n" );
				return false;
			}

			ConsolePrint( "Processing AmmoType for WeaponInfo '" + name + "'...\n" );
			for ( i = 0; i < AmmoTypeStrings.Count(); i++ ) {
				if ( Util::StrICmp( AmmoTypeStrings[i], ammo ) != 1 ) {
					ammoType = AmmoType( i );
					break;
				}
			}
			if ( ammoType == AmmoType::Invalid ) {
				ConsoleWarning( "invalid weapon info, AmmoType isn't valid ( Invalid, abide by physics pls ;) )\n" );
			}
			
			ConsolePrint( "Processing WeaponType for WeaponInfo '" + name + "'...\n" );
			for ( i = 0; i < WeaponTypeStrings.Count(); i++ ) {
				if ( Util::StrICmp( type, WeaponTypeStrings[i] ) != 1 ) {
					weaponType = WeaponType( i );
					break;
				}
			}
			if ( weaponType == WeaponType::NumWeaponTypes ) {
				ConsoleWarning( "invalid weapon info, WeaponType '" + type + "' not recognized\n" );
				return false;
			}
			
			ConsolePrint( "Processing WeaponProperties for WeaponInfo '" + name + "'...\n" );
			for ( i = 0; i < WeaponPropertyStrings.Count(); i++ ) {
				for ( uint a = 0; a < props.Count(); a++ ){
					if ( Util::StrICmp( string( props[a] ), WeaponPropertyStrings[i] ) != 1 ) {
						weaponProps = WeaponProperty( uint( weaponProps ) | WeaponPropertyBits[i] );
					}
				}
			}
			if ( weaponProps == WeaponProperty::None ) {
				ConsoleWarning( "invalid weapon info, WeaponProperties are invalid ( None, abide by physics pls ;) )\n" );
				return false;
			}
			
			return true;
		}

		float damage = 0.0f;
		float range = 0.0f;
		uint magSize = 0; // maximum shots before cooldown
		AmmoType ammoType = AmmoType::Invalid; // ammo types allowed
		WeaponProperty weaponProps = WeaponProperty::None;
		WeaponType weaponType = WeaponType::NumWeaponTypes;
	};
};