#include "json_value.as"
#include "json_tokenizer.as"

namespace TheNomad::Util {
	class JsonObject {
		JsonObject() {
		}
		
		JsonValue@ Parse( const string& in buffer ) {
			pos = 0;
			tokens.resize( 0 );
			
			tokens = tokenizer.Parse( buffer );
			JsonValue@ head = Process();
			return @head;
		}
		
		JsonValue@ Process() {
			if ( tokens.length() < 1 ) {
				GameError( "JsonObject: no tokens found in text buffer" );
				return null;
			}
			
			switch ( tokens[pos].getType() ) {
			case JsonTokenType::BEGIN_OBJECT:
				return ParseObject();
			case JsonTokenType::BEGIN_ARRAY:
				return ParseArray();
			case JsonTokenType::NULL:
				return ParseNull();
			case JsonTokenType::NUMBER:
				return ParseNumber();
			case JsonTokenType::STRING:
				return ParseString();
			case JsonTokenType::BOOLEAN:
				return ParseBoolean();
			default:
				GameError( "invalid type " + tokens[pos].getType() );
			};
			
			return null;
		}
		
		bool IsElement( JsonTokenType type ) const {
			switch ( type ) {
			case JsonTokenType::BEGIN_OBJECT:
			case JsonTokenType::BEGIN_ARRAY:
			case JsonTokenType::NULL:
			case JsonTokenType::NUMBER:
			case JsonTokenType::STRING:
			case JsonTokenType::BOOLEAN:
				return true;
			default:
				break;
			};
			return false;
		}
		
		JsonValue@ ParseBoolean() {
			string val = tokens[pos].getValue();
			bool targetValue = StringToBool( val );
			return JsonValue( targetValue );
		}
		
		JsonValue@ ParseNumber() {
			string val = tokens[pos].getValue();
			
			// check if int or float
			if ( val.find( '.' ) == StringNPos ) {
				int targetValue = StringToInt( val );
				return JsonValue( targetValue );
			} else {
				float targetValue = StringToFloat( val );
				return JsonValue( targetValue );
			}
		}
		
		JsonValue@ ParseString() {
			return JsonValue( tokens[pos].getValue() );
		}
		
		JsonValue@ ParseObject() {
			dictionary result;
			
			while ( pos < tokens.length() && tokens[pos].getType() != JsonTokenType::END_OBJECT ) {
				if ( tokens[pos].getType() == JsonTokenType::SEP_COLON ) {
					if ( pos - 1 >= 0 && pos + 1 < tokens.length() && tokens[pos - 1].getType() == JsonTokenType::STRING
						&& IsElement( tokens[pos + 1].getType() ) )
					{
						// has a key
						pos++;
						result.set( tokens[pos - 2].getValue(), Process() );
					} else {
						GameError( "invlaid key-value pair in a json object" );
					}
				}
				pos++;
			}
			return JsonValue( @result );
		}
		
		JsonValue@ ParseArray() {
			array<JsonValue@> result;
			
			pos++;
			while ( pos < tokens.length() && tokens[pos].getType() != JsonTokenType::END_ARRAY ) {
				if ( tokens[pos].getType() == JsonTokenType::SEP_COMMA ) {
					pos++;
					continue;
				}
				if ( IsElement( tokens[pos].getType() ) ) {
					result.insertLast( Process() );
				} else {
					GameError( "invalid value in json array" );
				}
				pos++;
			}
			
			return JsonValue( @result );
		}
		
		string dump( const JsonValue& in head ) {
			string result;
			
			result.reserve( MAX_STRING_CHARS );
			switch ( head.type() ) {
			case JsonValueType::OBJECT_VALUE: {
				result += "{";
				dictionary val = dictionary( head );
				array<string> keys = val.getKeys();
				for ( uint i = 0; i < keys.size(); i++ ) {
					result += "\"" + keys[i] + "\"";
					result += ":";
					JsonValue@ temp = cast<JsonValue>( val[keys[i]] );
					result += dump( temp );
					if ( i != keys.size() - 1 ) {
						result += ",";
					}
				}
				result += "}";
				break; }
			case JsonValueType::ARRAY_VALUE: {
				result += "[";
				
				array<JsonValue@> val = array<JsonValue@>( head );
				
				for ( uint i = 0; i < val.size(); i++ ) {
					result += dump( val[i] );
					if ( i != val.size() - 1 ) {
						result += ",";
					}
				}
				result += "]";
				break; }
			case JsonValueType::BOOLEAN_VALUE: {
				result += BoolToString( bool( head ) );
				break; }
			case JsonValueType::STRING_VALUE: {
				result += "\"";
				result += string( head );
				result += "\"";
				break; }
			case JsonValueType::INT_VALUE: {
				result += formatInt( int( head ) );
				break; }
			case JsonValueType::REAL_VALUE: {
				result += formatFloat( float( head ) );
				break; }
			case JsonValueType::NULL_VALUE: {
				result += "null";
				break; }
			default:
				GameError( "invalid json type" );
			};
			
			return result;
		}
		
		JsonValue@ ParseNull() {
			return JsonValue();
		}
		
		private uint pos = 0;
		private array<JsonToken@> tokens;
		private JsonTokenizer tokenizer;
	};
};