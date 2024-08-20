namespace TheNomad::Util {
	enum JsonTokenType {
		BEGIN_OBJECT,
		END_OBJECT,
		BEGIN_ARRAY,
		END_ARRAY,
		NULL,
		NUMBER,
		STRING,
		BOOLEAN,
		SEP_COLON,
		SEP_COMMA,
		END_DOCUMENT,
		ERROR
	};
	
	class JsonToken {
		JsonToken( JsonTokenType tokenType = JsonTokenType::NULL, const string& in value = "" ) {
			m_TokenType = tokenType;
			m_Value = value;
		}
		JsonToken( JsonTokenType tokenType = JsonTokenType::NULL, char value = '' ) {
			m_TokenType = tokenType;
			m_Value = value;
		}
		
		JsonTokenType getType() const {
			return m_TokenType;
		}
		
		const string& getValue() const {
			return m_Value;
		}
		string& getValue() {
			return m_Value;
		}
		
		private JsonTokenType m_TokenType;
		private string m_Value;
	};
	
	class JsonTokenizer {
		JsonTokenizer( bool bShouldConvertEscape = true ) {
			shouldConvertEscape = bShouldConvertEscape;
		}
		
		array<JsonToken@>@ Parse( const string& in buffer ) {
			dataStr = buffer;
			Tokenize();
			return @tokens;
		}
		
		array<JsonToken@>@ Parse( const string& in str, bool bShouldConvertEscape ) {
			shouldConvertEscape = bShouldConvertEscape;
			return Parse( str );
		}

		private void Tokenize() {
			JsonToken@ currentToken;
			
			tokens.resize( 0 );
			pos = 0;
			line = 0;
			do {
				@currentToken = Process();
				tokens.insertLast( @currentToken );
			} while ( currentToken.getType() != JsonTokenType::END_DOCUMENT && currentToken.getType() != JsonTokenType::ERROR );
			
			if ( currentToken.getType() == JsonTokenType::ERROR ) {
				GameError( "an error occurred while trying to tokenize JSON string " + currentToken.getValue() + "\n" );
			}
		}
		
		private char getNextChar() {
			buffer = dataStr.substr( pos, 1 )[0];
			
			if ( pos < dataStr.size() ) {
				pos++;
			}
			if ( buffer == '\n' ) {
				line++;
				col = 1;
			} else {
				col++;
			}
			return buffer;
		}
		
		private char peekChar( int offset = 0 ) {
			uint relativePos = pos + offset;
			
			if ( relativePos > 0 && relativePos < dataStr.size() ) {
				return dataStr[relativePos];
			}
			return buffer;
		}
		
		private JsonToken Process() {
			while ( true ) {
				if ( pos >= dataStr.size() ) {
					return JsonToken( JsonTokenType::END_DOCUMENT, "" );
				}
				getNextChar();
				if ( !IsSpace( buffer ) ) {
					break;
				}
			}
			
			switch ( buffer ) {
			case '{': return JsonToken( JsonTokenType::BEGIN_OBJECT, buffer );
			case '}': return JsonToken( JsonTokenType::END_OBJECT, buffer );
			case '[': return JsonToken( JsonTokenType::BEGIN_ARRAY, buffer );
			case ']': return JsonToken( JsonTokenType::END_ARRAY, buffer );
			case ',': return JsonToken( JsonTokenType::SEP_COMMA, buffer );
			case ':': return JsonToken( JsonTokenType::SEP_COLON, buffer );
			case 'n': return readNull();
			case 't': return readBoolean( true );
			case 'f': return readBoolean( false );
			case '\"': return readString( true );
			case '-': return readNumber();
			default:
				if ( IsDigit( buffer ) ) {
					return readNumber();
				} else if ( IsAlpha( buffer ) ) {
					return readString( false );
				}
				break;
			};
			return JsonToken( JsonTokenType::NULL, "" );
		}
		
		private JsonToken readNull() {
			if ( !( getNextChar() == 'u' && getNextChar() == 'l' && getNextChar() == 'l' ) ) {
				pos -= 3;
				return readString( false );
			}
			return JsonToken( JsonTokenType::NULL, "" );
		}
		
		private JsonToken readBoolean( bool expected ) {
			string tmp;
			uint i;
			
			if ( expected ) {
				tmp = "t";
				for ( i = 0; i < 3; i++ ) {
				  tmp += getNextChar();
				}
			} else {
				tmp = "f";
				for ( i = 0; i < 4; i++ ) {
					tmp += getNextChar();
				}
			}
			if ( expected ) {
				if ( tmp == "true" ) {
					return JsonToken( JsonTokenType::BOOLEAN, "true" );
				} else {
					pos -= 3;
					return readString( false );
				}
			} else {
				if ( tmp == "false" ) {
					return JsonToken( JsonTokenType::BOOLEAN, "false" );
				} else {
					pos -= 4;
					return readString( false );
				}
			}
		}
		
		private JsonToken readString( bool isStandard ) {
			array<string> endingChar;
			string strStorage;
			uint i;
			
			int initLine = line;
			int initCol = col;
			
			if ( isStandard ) {
				strStorage = "";
			} else {
				strStorage = peekChar( -1 );
			}
			
			while ( true ) {
				char next = getNextChar();
				
				if ( !isStandard && IsSpace( buffer ) ) {
					continue;
				}
				if ( pos < dataStr.size() ) {
					bool shouldEnd = false;
					bool isEndTypeStandard = isStandard;
					if ( buffer == '\"' ) {
						shouldEnd = true;
						isEndTypeStandard = true;
					}
					if ( buffer == ':' ) {
						shouldEnd = true;
						isEndTypeStandard = false;
					}
					if ( !shouldEnd ) {
						if ( buffer == '\\' && shouldConvertEscape ) {
							strStorage += convertEscape( isStandard );
						} else {
							strStorage += buffer;
						}
					} else {
				  		if ( isEndTypeStandard != isStandard ) {
							return JsonToken( JsonTokenType::ERROR, "(" + initLine + ":" + initCol + "): unexpected end of string '" + buffer + "'" );
						}
						if ( !isStandard ) {
							pos--;
						}
						break;
					}
				} else {
					return JsonToken( JsonTokenType::ERROR, "(" + initLine + ":" + initCol + "): no end of string" );
				}
			}
			return JsonToken( JsonTokenType::STRING, strStorage );
		}

    	private string convertEscape( bool isStandard ) {
			string storageStr;
			uint i;
			char next = getNextChar();
			
			// BUG: Following escape characters are currently not available in AngelScript: \f \b
			switch ( next ) {
			case '\"':
			case '\'':
			case '\\':
			case '\n':
			case '\r':
			case '\t':
			case '/': {
				string str;
				str.resize( 1 );
				str[0] = next;
				return str; }
			default:
				break;
			};
			
			if ( next == 'u' ) {
				storageStr = "\\u";
				while ( true ) {
					getNextChar();
					if ( pos < dataStr.size() -  1 ) {
						bool shouldEnd = false;
						if ( buffer == '\"' || buffer == ':' ) {
							shouldEnd = true;
						}
						if ( shouldEnd ) {
							pos--;
							return storageStr;
						}
						storageStr += buffer;
						if ( storageStr.size() >= 6 ) {
							// BUG: storageStr here is already an escape character,
							// but cannot be converted to a unicode character. This
							// is due to the limitation of AngelScript.
							return storageStr;
						}
					} else {
						storageStr += buffer;
						return storageStr;
					}
				}
			}
			pos--;
			return "\\";
		}
		
		private JsonToken readNumber() {
			string num = buffer;
			
			num.reserve( 64 );
			while ( true ) {
				char next = getNextChar();
				if ( pos < dataStr.size() ) {
					if ( IsDigit( next ) || next == '.' ) {
						num += next;
					} else {
						pos--;
						break;
					}
				} else {
					break;
				}
			}
			return JsonToken( JsonTokenType::NUMBER, num );
		}
		
		private int pos = 0;
		private uint line = 1;
		private uint col = 1;
		private string dataStr = "";
		private char buffer = 0;
		private bool shouldConvertEscape = true;
		private array<JsonToken@> tokens;
	};
};