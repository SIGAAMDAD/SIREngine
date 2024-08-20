namespace TheNomad::GameSystem::SaveSystem {
    class SaveSection {
		SaveSection( const string& in name ) {
			BeginSaveSection( name );
		}
		~SaveSection() {
			EndSaveSection();
		}
		
		void SaveVec2( const string& in name, const vec2& in value ) const {
			this.SaveFloat( "vec2_" + name + ".x", value.x );
			this.SaveFloat( "vec2_" + name + ".y", value.y );
		}
		void SaveVec3( const string& in name, const vec3& in value ) const {
			this.SaveFloat( "vec3_" + name + ".x", value.x );
			this.SaveFloat( "vec3_" + name + ".y", value.y );
			this.SaveFloat( "vec3_" + name + ".z", value.z );
		}
        void SaveVec4( const string& in name, const vec4& in value ) const {
            this.SaveFloat( "vec4_" + name + ".r", value.r );
            this.SaveFloat( "vec4_" + name + ".g", value.g );
            this.SaveFloat( "vec4_" + name + ".b", value.b );
            this.SaveFloat( "vec4_" + name + ".a", value.a );
        }

		void SaveBool( const string& in name, bool value ) const {
			TheNomad::GameSystem::SaveUInt( name, Convert().ToUInt( value ) );
		}

		void SaveFloat( const string& in name, float value ) const {
			TheNomad::GameSystem::SaveFloat( name, value );
		}
		void SaveArray( const string& in name, const array<uint>& in value ) const {
			TheNomad::GameSystem::SaveArray( name, value );
		}
		void SaveArray( const string& in name, const array<int>& in value ) const {
			TheNomad::GameSystem::SaveArray( name, value );
		}
		void SaveArray( const string& in name, const array<float>& in value ) const {
			TheNomad::GameSystem::SaveArray( name, value );
		}
		void SaveString( const string& in name, const string& in value ) const {
			TheNomad::GameSystem::SaveString( name, value );
		}
		void SaveChar( const string& in name, int8 value ) const {
			TheNomad::GameSystem::SaveInt8( name, value );
		}
		void SaveShort( const string& in name, int16 value ) const {
			TheNomad::GameSystem::SaveInt16( name, value );
		}
		void SaveInt( const string& in name, int32 value ) const {
			TheNomad::GameSystem::SaveInt32( name, value );
		}
		void SaveLong( const string& in name, int64 value ) const {
			TheNomad::GameSystem::SaveInt64( name, value );
		}
		void SaveByte( const string& in name, uint8 value ) const {
			TheNomad::GameSystem::SaveInt8( name, value );
		}
		void SaveUShort( const string& in name, uint16 value ) const {
			TheNomad::GameSystem::SaveUInt16( name, value );
		}
		void SaveUInt( const string& in name, uint32 value ) const {
			TheNomad::GameSystem::SaveUInt32( name, value );
		}
		void SaveULong( const string& in name, uint64 value ) const {
			TheNomad::GameSystem::SaveUInt64( name, value );
		}
		void SaveInt8( const string& in name, int8 value ) const {
			TheNomad::GameSystem::SaveInt8( name, value );
		}
		void SaveInt16( const string& in name, int16 value ) const {
			TheNomad::GameSystem::SaveInt16( name, value );
		}
		void SaveInt32( const string& in name, int32 value ) const {
			TheNomad::GameSystem::SaveInt32( name, value );
		}
		void SaveInt64( const string& in name, int64 value ) const {
			TheNomad::GameSystem::SaveInt64( name, value );
		}
		void SaveUInt8( const string& in name, uint8 value ) const {
			TheNomad::GameSystem::SaveUInt8( name, value );
		}
		void SaveUInt16( const string& in name, uint16 value ) const {
			TheNomad::GameSystem::SaveUInt16( name, value );
		}
		void SaveUInt32( const string& in name, uint32 value ) const {
			TheNomad::GameSystem::SaveUInt32( name, value );
		}
		void SaveUInt64( const string& in name, uint64 value ) const {
			TheNomad::GameSystem::SaveUInt64( name, value );
		}
	};
};