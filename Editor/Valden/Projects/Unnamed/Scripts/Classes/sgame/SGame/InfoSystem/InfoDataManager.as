#include "SGame/InfoSystem/InfoLoader.as"
#include "SGame/InfoSystem/AttackInfo.as"
#include "SGame/InfoSystem/MobInfo.as"
#include "SGame/InfoSystem/ItemInfo.as"
#include "SGame/InfoSystem/WeaponInfo.as"
#include "SGame/InfoSystem/AmmoInfo.as"

namespace TheNomad::SGame::InfoSystem {
    class InfoDataManager {
		InfoDataManager() {
			ConsolePrint( "Loading mod info files...\n" );

			LoadEntityIds();
		}

		private array<json@>@ LoadJSonFile( string& in path, const string& in modName, const string& in fileName,
			const string& in ArrayName )
		{
			json@ data;
			array<json@> values;
			
			path = "modules/" + modName + "/DataScripts/" + fileName;

			@data = json();
			if ( !data.ParseFile( path ) ) {
				return null;
			}
			if ( !data.get( ArrayName, values ) ) {
				ConsoleWarning( "info file '" + fileName + "' found, but no infos defined, skipping...\n" );
				return null;
			}
			
			return @values;
		}
		
		private void LoadEntityIds() {
			string path;
			json@ data;
			json@ obj;
			array<json@> values;
			string name;
			uint id;

			@data = json();
			for ( uint i = 0; i < sgame_ModList.Count(); i++ ) {
				path = "modules/" + sgame_ModList[i] + "/DataScripts/entitydata.json";
				if ( !data.ParseFile( path ) ) {
					ConsoleWarning( "failed to load entity data info file for \"" + sgame_ModList[i] + "\"\n" );
					return;
				}

				//
				// load the mob infos
				//
				if ( !data.get( "MobData", values ) ) {
					ConsoleWarning( "entity data info file for \"" + sgame_ModList[i] + "\" has no mob data\n" );
				} else {
					for ( uint a = 0; a < values.Count(); a++ ) {
						if ( !values[a].get( "Name", name ) ) {
							GameError( "invalid entity data info file, MobData object missing variable 'Name'" );
						}
						if ( !values[a].get( "Id", id ) ) {
							GameError( "invalid entity data info file, MobData object missing variable 'Id'" );
						}
						m_MobTypes[ name ] = id;
					}
				}
				DebugPrint( "Loaded " + m_MobTypes.Count() + " mob types.\n" );

				//
				// load the ammo infos
				//
				if ( !data.get( "AmmoData", values ) ) {
					ConsoleWarning( "entity data info file for \"" + sgame_ModList[i] + "\" has no ammo data\n" );
				} else {
					for ( uint a = 0; a < values.Count(); a++ ) {
						if ( !values[a].get( "Name", name ) ) {
							GameError( "invalid entity data info file, AmmoData object missing variable 'Name'" );
						}
						if ( !values[a].get( "Id", id ) ) {
							GameError( "invalid entity data info file, AmmoData object missing variable 'Id'" );
						}
						m_AmmoTypes[ name ] = id;
					}
				}
				DebugPrint( "Loaded " + m_AmmoTypes.Count() + " ammo types.\n" );

				//
				// load item infos
				//
				if ( !data.get( "ItemData", values ) ) {
					ConsoleWarning( "entity data info file for \"" + sgame_ModList[i] + "\" has no item data\n" );
				} else {
					for ( uint a = 0; a < values.Count(); a++ ) {
						if ( !values[a].get( "Name", name ) ) {
							GameError( "invalid entity data info file, ItemData object missing variable 'Name'" );
						}
						if ( !values[a].get( "Id", id ) ) {
							GameError( "invalid entity data info file, ItemData object missing variable 'Id'" );
						}
						m_ItemTypes[ name ] = id;
					}
				}
				DebugPrint( "Loaded " + m_ItemTypes.Count() + " item types.\n" );

				//
				// load weapon infos
				//
				if ( !data.get( "WeaponData", values ) ) {
					ConsoleWarning( "entity data info file for \"" + sgame_ModList[i] + "\" has no weapon data\n" );
				} else {
					for ( uint a = 0; a < values.Count(); a++ ) {
						if ( !values[a].get( "Name", name ) ) {
							GameError( "invalid entity data info file, WeaponData object missing variable 'Name'" );
						}
						if ( !values[a].get( "Id", id ) ) {
							GameError( "invalid entity data info file, WeaponData object missing variable 'Id'" );
						}
						m_WeaponTypes[ name ] = id;
					}
				}
				DebugPrint( "Loaded " + m_WeaponTypes.Count() + " weapon datas.\n" );
			}
		}
		
		void LoadMobInfos() {
			array<json@>@ infos;
			MobInfo@ info;
			string path;

			for ( uint i = 0; i < sgame_ModList.Count(); i++ ) {
				ConsolePrint( "Loading mob infos from module \"" + sgame_ModList[i] + "\"...\n" );

				@infos = @LoadJSonFile( path, sgame_ModList[i], "mobs.json", "MobInfo" );
				if ( @infos is null ) {
					return;
				}
				ConsolePrint( "Got " + infos.Count() + " mob infos.\n" );
			
				for ( uint a = 0; a < infos.Count(); a++ ) {
					@info = MobInfo();
					if ( !info.Load( @infos[a] ) ) {
						ConsoleWarning( "failed to load mob info at " + a + "\n" );
						continue;
					}

					m_MobInfos.Add( @info );
				}
			}
		}
		
		void LoadItemInfos() {
			array<json@>@ infos;
			ItemInfo@ info;
			string path;

			for ( uint i = 0; i < sgame_ModList.Count(); i++ ) {
				ConsolePrint( "Loading item infos from module \"" + sgame_ModList[i] + "\"...\n" );

				@infos = @LoadJSonFile( path, sgame_ModList[i], "items.json", "ItemInfo" );
				if ( @infos is null ) {
					return;
				}
			
				for ( uint a = 0; a < infos.Count(); a++ ) {
					@info = ItemInfo();
					if ( !info.Load( @infos[a] ) ) {
						ConsoleWarning( "failed to load item info at " + a + "\n" );
						continue;
					}
					
					m_ItemInfos.Add( @info );
				}
			}
		}

		void LoadAmmoInfos() {
			array<json@>@ infos;
			AmmoInfo@ info;
			string path;

			for ( uint i = 0; i < sgame_ModList.Count(); i++ ) {
				ConsolePrint( "Loading ammo infos from module \"" + sgame_ModList[i] + "\"...\n" );

				@infos = @LoadJSonFile( path, sgame_ModList[i], "ammo.json", "AmmoInfo" );
				if ( @infos is null ) {
					return;
				}
			
				for ( uint a = 0; a < infos.Count(); a++ ) {
					@info = AmmoInfo();
					if ( !info.Load( @infos[a] ) ) {
						ConsoleWarning( "failed to load ammo info at " + a + "\n" );
						continue;
					}
					
					m_AmmoInfos.Add( @info );
				}
			}
		}
		
		void LoadWeaponInfos() {
			array<json@>@ infos;
			WeaponInfo@ info;
			string path;

			for ( uint i = 0; i < sgame_ModList.Count(); i++ ) {
				ConsolePrint( "Loading weapon infos from module \"" + sgame_ModList[i] + "\"...\n" );

				@infos = @LoadJSonFile( path, sgame_ModList[i], "weapons.json", "WeaponInfo" );
				if ( @infos is null ) {
					return;
				}
			
				for ( uint a = 0; a < infos.Count(); a++ ) {
					@info = WeaponInfo();
					if ( !info.Load( @infos[a] ) ) {
						ConsoleWarning( "failed to load weapon info at " + a + "\n" );
						continue;
					}

					m_WeaponInfos.Add( @info );
				}
			}
		}

		AmmoInfo@ GetAmmoInfo( const string& in name ) {
			for ( uint i = 0; i < m_AmmoInfos.Count(); i++ ) {
				if ( Util::StrICmp( m_AmmoInfos[i].name, name ) == 0 ) {
					return m_AmmoInfos[i];
				}
			}
			return null;
		}
		
		WeaponInfo@ GetWeaponInfo( const string& in name ) {
			for ( uint i = 0; i < m_WeaponInfos.Count(); i++ ) {
				if ( Util::StrICmp( m_WeaponInfos[i].name, name ) == 0 ) {
					return m_WeaponInfos[i];
				}
			}
			return null;
		}
		
		ItemInfo@ GetItemInfo( const string& in name ) {
			for ( uint i = 0; i < m_ItemInfos.Count(); i++ ) {
				if ( Util::StrICmp( m_ItemInfos[i].name, name ) == 0 ) {
					return m_ItemInfos[i];
				}
			}
			return null;
		}
		
		MobInfo@ GetMobInfo( const string& in name ) {
			for ( uint i = 0; i < m_MobInfos.Count(); i++ ) {
				if ( Util::StrICmp( m_MobInfos[i].name, name ) == 0 ) {
					return m_MobInfos[i];
				}
			}
			return null;
		}
		
		bool WeaponInfoExists( const string& in name ) const {
			for ( uint i = 0; i < m_WeaponInfos.Count(); i++ ) {
				if ( Util::StrICmp( m_WeaponInfos[i].name, name ) == 0 ) {
					return true;
				}
			}
			return false;
		}
		
		bool ItemInfoExists( const string& in name ) const {
			for ( uint i = 0; i < m_ItemInfos.Count(); i++ ) {
				if ( Util::StrICmp( m_ItemInfos[i].name, name ) == 0 ) {
					return true;
				}
			}
			return false;
		}
		
		bool MobInfoExists( const string& in name ) const {
			for ( uint i = 0; i < m_MobInfos.Count(); i++ ) {
				if ( Util::StrICmp( m_MobInfos[i].name, name ) == 0 ) {
					return true;
				}
			}
			return false;
		}

		WeaponInfo@ GetWeaponInfo( uint id ) {
			for ( uint i = 0; i < m_WeaponInfos.Count(); i++ ) {
				if ( m_WeaponInfos[i].type == id ) {
					return @m_WeaponInfos[i];
				}
			}
			return null;
		}
		
		AmmoInfo@ GetAmmoInfo( AmmoType id ) {
			for ( uint i = 0; i < m_AmmoInfos.Count(); i++ ) {
				if ( ( m_AmmoInfos[i].baseType & id ) != 0 ) {
					return @m_AmmoInfos[i];
				}
			}
			return null;
		}

		ItemInfo@ GetItemInfo( uint id ) {
			for ( uint i = 0; i < m_ItemInfos.Count(); i++ ) {
				if ( m_ItemInfos[i].type == id ) {
					return @m_ItemInfos[i];
				}
			}
			return null;
		}

		MobInfo@ GetMobInfo( uint id ) {
			for ( uint i = 0; i < m_MobInfos.Count(); i++ ) {
				if ( m_MobInfos[i].type == id ) {
					return @m_MobInfos[i];
				}
			}
			return null;
		}

		dictionary@ GetItemTypes() {
			return @m_ItemTypes;
		}
		dictionary@ GetMobTypes() {
			return @m_MobTypes;
		}
		dictionary@ GetWeaponTypes() {
			return @m_WeaponTypes;
		}
		dictionary@ GetAmmoTypes() {
			return @m_AmmoTypes;
		}

		const dictionary@ GetItemTypes() const {
			return @m_ItemTypes;
		}
		const dictionary@ GetMobTypes() const {
			return @m_MobTypes;
		}
		const dictionary@ GetWeaponTypes() const {
			return @m_WeaponTypes;
		}
		const dictionary@ GetAmmoTypes() const {
			return @m_AmmoTypes;
		}
		
		void AddMobInfo( MobInfo@ info ) {
			m_MobInfos.Add( info );
		}
		void AddItemInfo( ItemInfo@ info ) {
			m_ItemInfos.Add( info );
		}
		void AddWeaponInfo( WeaponInfo@ info ) {
			m_WeaponInfos.Add( info );
		}

		private dictionary m_ItemTypes;
		private dictionary m_MobTypes;
		private dictionary m_WeaponTypes;
		private dictionary m_AmmoTypes;
		
		private array<MobInfo@> m_MobInfos;
		private array<ItemInfo@> m_ItemInfos;
		private array<WeaponInfo@> m_WeaponInfos;
		private array<AmmoInfo@> m_AmmoInfos;
	};

	enum ItemType {
		Powerup,
		Consumable,

		NumItemTypes
	};

    enum AttackMethod {
		Hitscan,
		RayCast, // for melee without the AOE
		Projectile,
		AreaOfEffect,

		NumAttackMethods
	};

	enum ArmorType {
		None,
		Light,
		Standard,
		Heavy,
		Invul,

		NumArmorTypes
	};

	enum AmmoType {
		Bullet,
		Shell,
		Rocket,
		Grenade,
		Invalid,

		NumAmmoTypes,
	};

	enum AttackType {
		Melee,
		Missile,

		NumAttackTypes
	};
	
	enum WeaponType {
		Sidearm,
		HeavySidearm,
		Primary,
		HeavyPrimary,
		Grenadier,
		Melee,
		LeftArm,
		RightArm,
		
		NumWeaponTypes
	};

	enum MobFlags {
		Deaf      = 0x0001,
		Blind     = 0x0002,
		Terrified = 0x0004,
		Boss      = 0x0008,
		Sentry    = 0x0010,
		PermaDead = 0x0020,

		None      = 0x0000
	};

	enum AmmoProperty {
		Heavy               = 0x0001,
		Light               = 0x0002,
		Pellets             = 0x0003,
		TypeBits            = 0x000f,

		NoPenetration       = 0x0010,
		ArmorPiercing       = 0x0020,
		HollowPoint         = 0x0030,
		PenetrationBits     = 0x00f0,

		Flechette           = 0x0100,
		Buckshot            = 0x0200,
		Shrapnel            = 0x0300,
		Slug                = 0x0400,
		ShotgunBullshitBits = 0x0f00,

		Explosive           = 0x1000,
		Incendiary          = 0x2000,
		Tracer              = 0x3000,
		ExtBulletBits       = 0xf000,

		None                = 0x0000
	};

	enum WeaponProperty {
		OneHandedBlade       = 0x00000101,
		OneHandedBlunt       = 0x00001002,
		OneHandedPolearm     = 0x00010003,
		OneHandedSideFirearm = 0x00100004,
		OneHandedPrimFirearm = 0x00100005,

		TwoHandedBlade       = 0x00000110,
		TwoHandedBlunt       = 0x00001020,
		TwoHandedPolearm     = 0x00010030,
		TwoHandedSideFirearm = 0x00100040,
		TwoHandedPrimFirearm = 0x00100050,

		IsOneHanded          = 0x0000000f,
		IsTwoHanded          = 0x000000f0,
		IsBladed             = 0x00000f00,
		IsBlunt              = 0x0000f000,
		IsPolearm            = 0x000f0000,
		IsFirearm            = 0x00f00000,

		SpawnsObject         = 0x10000000,

		None                 = 0x00000000
	};

	const string[] AttackTypeStrings = {
		"Melee",
		"Missile"
	};

	const uint[] MobFlagBits = {
		uint( MobFlags::Deaf ),
		uint( MobFlags::Blind ),
		uint( MobFlags::Terrified ),
		uint( MobFlags::Boss ),
		uint( MobFlags::Sentry )
	};

	const string[] MobFlagStrings = {
		"Deaf",
		"Blind",
		"Terrified",
		"Boss",
		"Sentry"
	};

	const string[] ItemTypeStrings = {
		"Powerup",
		"Consumable"
	};
	
	const uint[] WeaponPropertyBits = {
		uint( WeaponProperty::TwoHandedBlade ),
		uint( WeaponProperty::OneHandedBlade ),
		uint( WeaponProperty::TwoHandedBlunt ),
		uint( WeaponProperty::OneHandedBlunt ),
		uint( WeaponProperty::TwoHandedPolearm ),
		uint( WeaponProperty::OneHandedPolearm ),
		uint( WeaponProperty::OneHandedSideFirearm ),
		uint( WeaponProperty::TwoHandedSideFirearm ),
		uint( WeaponProperty::OneHandedPrimFirearm ),
		uint( WeaponProperty::TwoHandedPrimFirearm ),
		uint( WeaponProperty::SpawnsObject )
	};
	
	const string[] WeaponPropertyStrings = {
		"TwoHandedBlade",
		"OneHandedBlade",
		"TwoHandedBlunt",
		"OneHandedBlunt",
		"TwoHandedPolearm",
		"OneHandedPolearm",
		"OneHandedSidearmFirearm",
		"TwoHandedSidearmFirearm",
		"OneHandedPrimaryFirearm",
		"TwoHandedPrimaryFirearm",
		"SpawnsObject"
	};

	const string[] AmmoPropertyStrings = {
		"Heavy",
		"Light",
		"Pellets",

		"NoPenetration",
		"ArmorPiercing",
		"HollowPoint",

		"Flechette",
		"Buckshot",
		"Shrapnel",
		"Slug",

		"Explosive",
		"Incendiary",
		"Tracer"
	};

	const string[] AmmoTypeStrings = {
		"Bullet",
		"Shell",
		"Grenade",
		"Rocket"
	};
	const string[] ArmorTypeStrings = {
		"None",
		"Light",
		"Standard",
		"Heavy",
		"Invul"
	};
	const string[] AttackMethodStrings = {
		"Hitscan",
		"RayCast",
		"Projectile",
		"AreaOfEffect"
	};
	const string[] WeaponTypeStrings = {
		"Sidearm",
		"HeavySidearm",
		"Primary",
		"HeavyPrimary",
		"Grenadier",
		"Melee",
		"LeftArm",
		"RightArm"
	};

    InfoDataManager@ InfoManager;
};