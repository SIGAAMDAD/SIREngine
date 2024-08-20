namespace TheNomad::SGame::InfoSystem {
    class AmmoInfo : InfoLoader {
        AmmoInfo() {
        }

        //
        // PropertyIndexToBit: for some reason, AngelScript has a bug where this specific array
        // always has size 1, therefore out-of-range error
        //
        private uint PropertyIndexToBit( uint nIndex ) const {
            switch ( nIndex ) {
            case 0: return AmmoProperty::Heavy;
            case 1: return AmmoProperty::Light;
            case 2: return AmmoProperty::Pellets;
            case 3: return AmmoProperty::NoPenetration;
            case 4: return AmmoProperty::ArmorPiercing;
            case 5: return AmmoProperty::HollowPoint;
            case 6: return AmmoProperty::Flechette;
            case 7: return AmmoProperty::Buckshot;
            case 8: return AmmoProperty::Shrapnel;
            case 9: return AmmoProperty::Slug;
            case 10: return AmmoProperty::Explosive;
            case 11: return AmmoProperty::Incendiary;
            case 12: return AmmoProperty::Tracer;
            default:
                GameError( "AmmoInfo::PropertyIndexToBit: invalid ammo property index" );
                break;
            };
            return AmmoProperty::None;
        }

        bool Load( json@ json ) {
            array<json@> values;
            string type;
            string str;
            uint i, a;

            if ( !json.get( "Name", name ) ) {
                ConsoleWarning( "invalid ammo info, missing variable 'Name'\n" );
                return false;
            }
            if ( !json.get( "Type", type ) ) {
                ConsoleWarning( "invalid ammo info, missing variable 'Type'\n" );
                return false;
            }
            if ( !json.get( "Id", str ) ) {
                ConsoleWarning( "invalid ammo info, missing variable 'Id'\n" );
                return false;
            } else {
                if ( !InfoManager.GetAmmoTypes().TryGetValue( str, id ) ) {
					GameError( "invalid ammo info, Type \"" + str + "\" wasn't found" );
				}
            }

            // not really required, but it does make things more entertaining
            if ( !json.get( "Properties", values ) ) {
                ConsoleWarning( "ammo info \"" + id + "\" has no extra properties.\n" );
            }

            ConsolePrint( "Processing Properties for AmmoInfo '" + name + "'...\n" );
            for ( i = 0; i < AmmoPropertyStrings.Count(); i++ ) {
                for ( a = 0; a < values.Count(); a++ ) {
                    if ( Util::StrICmp( string( values[a] ), AmmoPropertyStrings[i] ) != 1 ) {
                        bits = AmmoProperty( uint( bits ) | PropertyIndexToBit( i ) );
                    }
                }
            }
            if ( bits == AmmoProperty::None ) {
                ConsolePrint( "ammo info \"" + id + "\" has no valid properties.\n" );
            }

            ConsolePrint( "Processing Type for AmmoInfo '" + name + "'...\n" );
            for ( i = 0; i < AmmoTypeStrings.Count(); i++ ) {
                if ( Util::StrICmp( AmmoTypeStrings[i], type ) != 1 ) {
                    baseType = AmmoType( i );
                    break;
                }
            }
            if ( baseType == AmmoType::Invalid ) {
                ConsoleWarning( "invalid ammo info, no base type.\n" );
                return false;
            }

            return true;
        }

        string name;
        uint id = 0;
        AmmoType baseType = AmmoType::Invalid;
        AmmoProperty bits;
    };
};