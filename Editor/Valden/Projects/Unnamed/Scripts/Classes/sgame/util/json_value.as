namespace TheNomad::Util {
	enum JsonValueType {
		OBJECT_VALUE,
		ARRAY_VALUE,
		BOOLEAN_VALUE,
		STRING_VALUE,
		INT_VALUE,
		REAL_VALUE,
		NULL_VALUE
	};
	
	
	class JsonValue {
		JsonValue() {
			set();
		}
		JsonValue( bool value ) {
			set( value );
		}
		JsonValue( const string& in value ) {
			set( value );
		}
		JsonValue( int value ) {
			set( value );
		}
		JsonValue( float value ) {
			set( value );
		}
		JsonValue( const array<JsonValue@>& in value ) {
			set( value );
		}
		JsonValue( const dictionary& in value ) {
			set( value );
		}
		JsonValue( dictionary@ value ) {
			set( @value );
		}
		JsonValue( array<JsonValue@>@ value ) {
			set( @value );
		}
    	
		private void Reset() {
			valueInt = 0;
			valueString = "";
			valueBoolean = false;
			valueReal = 0.0f;
			@valueArray = null;
			@valueObject = null;
		}
		
		JsonValueType type() const {
			return contentType;
		}
		bool opConv() const {
			return valueBoolean;
		}
		const string& opConv() const {
			return valueString;
		}
		int opConv() const {
			return valueInt;
		}
		float opConv() const {
			return valueReal;
		}
		const array<JsonValue@>@ opConv() const {
			return @valueArray;
		}
		const dictionary@ opConv() const {
			return @valueObject;
		}
		array<JsonValue@>@ opConv() {
			return @valueArray;
		}
		dictionary@ opConv() {
			return @valueObject;
		}

		void set() {
			Reset();
			contentType = JsonValueType::NULL_VALUE;
		}
		void set( const array<JsonValue@>& in value ) {
			Reset();
			contentType = JsonValueType::ARRAY_VALUE;
			if ( @valueArray is null ) {
				@valueArray = array<JsonValue@>();
			}
			valueArray = value;
		}
		void set( bool value ) {
			Reset();
			contentType = JsonValueType::BOOLEAN_VALUE;
			valueBoolean = value;
		}
		void set( const string& in value ) {
			Reset();
			contentType = JsonValueType::STRING_VALUE;
			valueString = value;
		}
		void set( int value ) {
			Reset();
			contentType = JsonValueType::INT_VALUE;
			valueInt = value;
		}
		void set( float value ) {
			Reset();
			contentType = JsonValueType::REAL_VALUE;
			valueReal = value;
		}
		void set( array<JsonValue@>@ value ) {
			Reset();
			contentType = JsonValueType::ARRAY_VALUE;
			@valueArray = @value;
		}
		void set( dictionary@ value ) {
			Reset();
			contentType = JsonValueType::OBJECT_VALUE;
			@valueObject = @value;
		}
		void set( const dictionary& in value ) {
			Reset();
			contentType = JsonValueType::OBJECT_VALUE;
			if ( @valueObject is null ){
				@valueObject = dictionary();
			}
			
			valueObject = value;
		}
		
		private bool IsANumber( const string& in str) {
		  bool val = true;
		  for (uint i = 0; i < str.size(); i++) {
		    if (!IsDigit(str.substr(i, 1)[0])) {
		      val = false;
		      break;
		    }
		  }
		  return val;
		}
		private JsonValue@ get_helper_arr( int idx ) {
			if ( idx >= int( valueArray.length() ) || idx < 0 ) {
				GameError( "index " + idx + " out of range for JsonValue array" );
				return JsonValue();
			} else {
				return valueArray[idx];
			}
		}
		
		private void set_helper_arr( int idx, const JsonValue@ value ) {
			if ( idx >= int( valueArray.length() ) || idx < 0 ) {
				GameError( "index " + idx + " out of range for JsonValue array" );
			} else {
				valueArray[idx] = value;
			}
		}

		JsonValue@ opIndex( int idx ) {
			if ( contentType != JsonValueType::ARRAY_VALUE ) {
				GameError( "JsonValue must be an array type to use an integer indexing operator" );
			}
			return get_helper_arr( idx );
		}
		
		JsonValue@ opIndex( const string& in idx ) {
			if ( contentType != JsonValueType::OBJECT_VALUE ) {
				if ( contentType == JsonValueType::ARRAY_VALUE ) {
					if ( IsANumber( idx ) ) {
						int idx_num = StringToInt( idx );
						return get_helper_arr( idx_num );
					} else {
						GameError( "JsonValue keys must be integers when accessing array values" );
					}
				} else {
					GameError( "JsonValue not an array or object" );
				}
				return JsonValue();
			}
			
			if ( !valueObject.exists( idx ) ) {
				ConsoleWarning( "out of range key '" + idx + "' for JsonValue object\n" );
				return JsonValue();
			}
			
			return cast<JsonValue@>( @valueObject[idx] );
		}
		
		void opIndex( const string& in idx, const JsonValue@ value ) {
			if ( contentType != JsonValueType::OBJECT_VALUE ) {
				if ( contentType == JsonValueType::ARRAY_VALUE ) {
					if ( IsANumber( idx ) ) {
						const int idx_num = StringToInt( idx );
						set_helper_arr( idx_num, value );
					} else {
						GameError( "JsonValue keys must be integers when accessing array values" );
					}
				} else {
					GameError( "JsonValue not an array or object" );
				}
			} else {
				valueObject[idx] = value;
			}
		}
		
		const string dump() const {
			JsonObject json;
			return json.dump( this );
		}
    
		private JsonValueType contentType = JsonValueType::NULL_VALUE;
		private int valueInt = 0;
		private string valueString = "";
		private bool valueBoolean = false;
		private float valueReal = 0.0f;
		private array<JsonValue@>@ valueArray = null;
		private dictionary@ valueObject = null;
	};
};