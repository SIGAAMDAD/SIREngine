/*
===============================================================================

	Fast hash table for indexes and arrays.
	Does not allocate memory until the first key/index pair is added.

===============================================================================
*/

namespace TheNomad::Engine::Containers {
    const uint DEFAULT_HASH_SIZE = 1024;
    const uint DEFAULT_HASH_GRANULARITY =1024;

    class HashIndex {
    public:
    	HashIndex() {
            Init( DEFAULT_HASH_SIZE, DEFAULT_HASH_SIZE );
        }
    	HashIndex( const int initialHashSize, const int initialIndexSize ) {
            Init( initialHashSize, initialIndexSize );
        }
    	~HashIndex() {
            Free();
        }

    	// returns total size of allocated memory
    	uint64 Allocated() const {
            return hashSize * 4 + indexSize * 4;
        }
    	
        // returns total size of allocated memory including size of hash index type
    	uint Size() const {
            return 116 + Allocated();
        }

    	HashIndex& opAssign( const HashIndex& in other ) {
            granularity = other.granularity;
        	hashMask = other.hashMask;
        	lookupMask = other.lookupMask;

        	if ( other.lookupMask == 0 ) {
        		hashSize = other.hashSize;
        		indexSize = other.indexSize;
        		Free();
        	}
        	else {
		        if ( other.hashSize != hashSize || hash == INVALID_INDEX ) {
		        	if ( hash.Count() != 0 ) {
                        hash.Clear();
		        	}
		        	hashSize = other.hashSize;
                    hash.Resize( hashSize );
		        }
		        if ( other.indexSize != indexSize || indexChain == INVALID_INDEX ) {
		        	if ( indexChain.Count() != 0 ) {
                        indexChain.Clear();
		        	}
		        	indexSize = other.indexSize;
                    indexChain.Resize( indexSize );
		        }

                for ( uint i = 0; i < hashSize; i++ ) {
                    hash[i] = other.hash[i];
                }
                for ( uint i = 0; i < indexSize; i++ ) {
                    indexChain[i] = other.indexChain[i];
                }
	        }

	        return this;
        }
    	
    	// add an index to the hash, assumes the index has not yet been added to the hash
    	void Add( const int key, const int index ) {
            int h;

            if ( index < 0 ) {
                GameError( "HashIndex::Add: index < 0" );
            }
        	if ( hash.Count() == 0 ) {
        		Allocate( hashSize, index >= indexSize ? index + 1 : indexSize );
        	}
        	else if ( index >= indexSize ) {
        		ResizeIndex( index + 1 );
        	}
        	h = key & hashMask;
        	indexChain[index] = hash[h];
        	hash[h] = index;
        }
    	
        // remove an index from the hash
    	void Remove( const int key, const int index ) {
            int k = key & hashMask;

        	if ( hash.Count() == 0 ) {
        		return;
        	}
        	if ( hash[k] == index ) {
        		hash[k] = indexChain[index];
        	}
        	else {
        		for ( int i = hash[k]; i != -1; i = indexChain[i] ) {
        			if ( indexChain[i] == index ) {
        				indexChain[i] = indexChain[index];
        				break;
        			}
        		}
        	}
        	indexChain[index] = -1;
        }
    	
        // get the first index from the hash, returns -1 if empty hash entry
    	int First( const int key ) const {
            return hash[ key & hashMask & lookupMask ];
        }

        // get the next index from the hash, returns -1 if at the end of the hash chain
    	int Next( const int index ) const {
            if ( index < 0 || index > indexSize ) {
                GameError( "HashIndex::Next: index < 0 || index > indexSize" );
            }
        	return indexChain[ index & lookupMask ];
        }
    	
        // insert an entry into the index and add it to the hash, increasing all indexes >= index
    	void InsertIndex( const int key, const int index ) {
            int i, max;

        	if ( hash.Count() != 0 ) {
        		max = index;
        		for ( i = 0; i < hashSize; i++ ) {
        			if ( hash[i] >= index ) {
        				hash[i]++;
        				if ( hash[i] > max ) {
        					max = hash[i];
        				}
        			}
        		}
        		for ( i = 0; i < indexSize; i++ ) {
        			if ( indexChain[i] >= index ) {
        				indexChain[i]++;
        				if ( indexChain[i] > max ) {
        					max = indexChain[i];
        				}
        			}
        		}
        		if ( max >= indexSize ) {
        			ResizeIndex( max + 1 );
        		}
        		for ( i = max; i > index; i-- ) {
        			indexChain[i] = indexChain[i-1];
        		}
        		indexChain[index] = -1;
        	}
        	Add( key, index );
        }
    	
        // remove an entry from the index and remove it from the hash, decreasing all indexes >= index
    	void RemoveIndex( const int key, const int index ) {
            int i, max;

        	Remove( key, index );
        	if ( hash.Count() != 0 ) {
        		max = index;
        		for ( i = 0; i < hashSize; i++ ) {
        			if ( hash[i] >= index ) {
        				if ( hash[i] > max ) {
        					max = hash[i];
        				}
        				hash[i]--;
        			}
        		}
        		for ( i = 0; i < indexSize; i++ ) {
        			if ( indexChain[i] >= index ) {
        				if ( indexChain[i] > max ) {
        					max = indexChain[i];
        				}
        				indexChain[i]--;
        			}
        		}
        		for ( i = index; i < max; i++ ) {
        			indexChain[i] = indexChain[i+1];
        		}
        		indexChain[max] = -1;
        	}
        }
    	
        // clear the hash
    	void Clear() {
            // only clear the hash table because clearing the indexChain is not really needed
        	if ( hash.Count() != 0 ) {
                for ( uint i = 0; i < hashSize; i++ ) {
                    hash[i] = 0xff;
                }
        	}
        }
    	
        // clear and resize
    	void Clear( const int newHashSize, const int newIndexSize ) {
            Free();
        	hashSize = newHashSize;
        	indexSize = newIndexSize;
        }
    	
        // free allocated memory
    	void Free() {
            if ( hash.Count() != 0 ) {
                hash.Clear();
        	}
        	if ( indexChain.Count() != 0 ) {
                indexChain.Clear();
        	}
        	lookupMask = 0;
        }
    	
        // get size of hash table
    	int GetHashSize() const {
            return hashSize;
        }
    	
        // get size of the index
    	int GetIndexSize() const {
            return indexSIze;
        }
    	
        // set granularity
    	void SetGranularity( const int newGranularity ) {
            if ( newGranularity < 0 ) {
                GameError( "HashIndex::SetGranularity: newGranularity < 0" );
            }
            granularity = newGranularity;
        }
    	
        // force resizing the index, current hash table stays intact
    	void ResizeIndex( const int newIndexSize ) {
            int[] oldIndexChain;
            int mod, newSize;

        	if ( newIndexSize <= indexSize ) {
        		return;
        	}

        	mod = newIndexSize % granularity;
        	if ( mod == 0 ) {
        		newSize = newIndexSize;
        	} else {
        		newSize = newIndexSize + granularity - mod;
        	}

        	if ( indexChain.Count() == 0 ) {
        		indexSize = newSize;
        		return;
        	}

        	oldIndexChain = indexChain;
            indexChain.Resize( newSize );
            for ( uint i = 0; i < indexSize; i++ ) {
                indexChain[i] = oldIndexChain[i];
            }
            for ( uint i = 0; i < ( newSize - indexSize ); i++ ) {
                indexChain[ i + indexSize ] = 0xff;
            }
            oldIndexChain.Clear();
        	indexSize = newSize;
        }
		
        // returns number in the range [0-100] representing the spread over the hash table
    	int	GetSpread() const {
            int i, index, totalItems;
            int[] numHashItems;
            int average, error, e;

        	if ( hash.Count() == 0 ) {
        		return 100;
        	}

        	totalItems = 0;
            numHashItems.Resize( hashSize );
        	for ( i = 0; i < hashSize; i++ ) {
        		numHashItems[i] = 0;
        		for ( index = hash[i]; index >= 0; index = indexChain[index] ) {
        			numHashItems[i]++;
        		}
        		totalItems += numHashItems[i];
        	}
        	// if no items in hash
        	if ( totalItems <= 1 ) {
                numHashItems.Clear();
        		return 100;
        	}
        	average = totalItems / hashSize;
        	error = 0;
        	for ( i = 0; i < hashSize; i++ ) {
        		e = abs( numHashItems[i] - average );
        		if ( e > 1 ) {
        			error += e - 1;
        		}
        	}
            numHashItems.Clear();
        	return 100 - ( error * 100 / totalItems );
        }
    	
        // returns a key for a string
    	int	GenerateKey( const string& in str, bool caseSensitive = true ) const {
            if ( caseSensitive ) {
    	    	return ( TheNomad::Util::HashString( str ) & hashMask );
	        } else {
		        return ( TheNomad::Util::IHashString( str ) & hashMask );
        	}
        }
    	
        // returns a key for a vector
    	int	GenerateKey( const vec3& in v ) const {
            return ( (((int) v[0]) + ((int) v[1]) + ((int) v[2])) & hashMask );
        }
    	
        // returns a key for two integers
    	int	GenerateKey( const int n1, const int n2 ) const {
            return ( ( n1 + n2 ) & hashMask );
        }

    	private int				hashSize;
    	private array<int>      hash;
    	private int				indexSize;
    	private array<int>		indexChain;
    	private int				granularity;
    	private int				hashMask;
    	private int				lookupMask;

    	private void Init( const int initialHashSize, const int initialIndexSize ) {
            if ( ( initialHashSize % 2 ) != 0 ) {
                GameError( "HashIndex::Init: initialHashSize not a power of two" );
            }

        	hashSize = initialHashSize;
        	hash = INVALID_INDEX;
        	indexSize = initialIndexSize;
        	indexChain = INVALID_INDEX;
        	granularity = DEFAULT_HASH_GRANULARITY;
        	hashMask = hashSize - 1;
        	lookupMask = 0;
        }
    	private void Allocate( const int newHashSize, const int newIndexSize ) {
            if ( ( newHashSize % 2 ) != 0 ) {
                GameError( "HashIndex::Init: newHashSize not a power of two" );
            }

            Free();
        	hashSize = newHashSize;
            hash.Resize( hashSize );
            for ( uint i = 0; i < hashSize; i++ ) {
                hash[i] = 0xff;
            }

        	indexSize = newIndexSize;
            indexChain.Resize( indexSize );
            for ( uint i = 0; i < indexSize; i++ ) {
                indexChain[i] = 0xff;
            }
        	hashMask = hashSize - 1;
        	lookupMask = -1;
        }
    };
};
