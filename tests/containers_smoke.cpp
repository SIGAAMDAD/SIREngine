#include <sir/core/SIRContainers.h>
#include <sir/core/SIRConstants.h>

#include <stdint.h>

int main( void )
{
	sir::SIRList<uint32_t> listValues;
	listValues.push_back( 7 );

	if ( listValues.size() != 1 || listValues[0] != 7 ) {
		return 1;
	}

	sir::SIRHashMap<uint32_t, uint32_t> hashCounts;
	hashCounts[7] = 12;

	if ( hashCounts[7] != 12 ) {
		return 2;
	}

	sir::SIRArray<uint8_t, 4> szBytes{};
	szBytes[0] = 1;

	if ( szBytes[0] != 1 || sir::SIR_CACHE_LINE_BYTES != 64 ) {
		return 3;
	}

	sir::SIRFixedList<uint32_t, 4> listFixed;
	listFixed.push_back( 5 );

	if ( listFixed[0] != 5 ) {
		return 4;
	}

	return 0;
}
