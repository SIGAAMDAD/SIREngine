#include <Core/Objects/SIRObjectRegistry.h>
#include <assert.h>
#include <string.h>

int main( void )
{
	sirObjectTypeDescriptor_t szTypes[4] = {};
	sirObjectPropertyDescriptor_t szProperties[4] = {};
	SIRObjectRegistry types;
	assert( types.initialize( szTypes, 4, szProperties, 4 ) == SIR_OBJECT_OK );
	assert( types.registerType( { 2, 1, 22 } ) == SIR_OBJECT_NOT_FOUND );
	assert( types.registerType( { 1, 0, 11 } ) == SIR_OBJECT_OK );
	assert( types.registerType( { 1, 0, 11 } ) == SIR_OBJECT_DUPLICATE_ID );
	assert( types.registerType( { 2, 1, 22 } ) == SIR_OBJECT_OK );
	assert( types.registerProperty( { 5, 1, 55, SIR_PROPERTY_BOOL, {} } ) == SIR_OBJECT_OK );
	assert( types.registerProperty( { 6, 2, 66, SIR_PROPERTY_REAL64, {} } ) == SIR_OBJECT_OK );
	assert( types.registerProperty( { 5, 2, 55, SIR_PROPERTY_BOOL, {} } ) == SIR_OBJECT_DUPLICATE_ID );
	sirPropertyValue_t value = { SIR_PROPERTY_BOOL, {}, 1 };
	assert( types.validateProperty( 2, 5, value ) == SIR_OBJECT_OK );
	value.nPayload = 2;
	assert( types.validateProperty( 2, 5, value ) == SIR_OBJECT_INVALID_ARGUMENT );
	value = { SIR_PROPERTY_INT64, {}, 1 };
	assert( types.validateProperty( 2, 5, value ) == SIR_OBJECT_TYPE_MISMATCH );
	value = { SIR_PROPERTY_REAL64, {}, 0 };
	double fValue = 2.5;
	memcpy( &value.nPayload, &fValue, sizeof( fValue ) );
	assert( types.validateProperty( 2, 6, value ) == SIR_OBJECT_OK );
	assert( types.seal() == SIR_OBJECT_OK );
	assert( types.registerType( { 3, 1, 33 } ) == SIR_OBJECT_SEALED );

	sirObjectSlot_t szSlots[2] = {};
	SIRObjectStore objects;
	assert( objects.initialize( szSlots, 2, &types ) == SIR_OBJECT_OK );
	sirObjectHandle_t first = {}, second = {};
	assert( objects.create( 42, 2, &first ) == SIR_OBJECT_OK );
	assert( objects.create( 42, 2, &second ) == SIR_OBJECT_DUPLICATE_ID );
	assert( objects.create( 43, 1, &second ) == SIR_OBJECT_OK );
	assert( objects.create( 44, 1, &second ) == SIR_OBJECT_OUT_OF_CAPACITY );
	sirObjectSlot_t observed = {};
	assert( objects.resolve( first, &observed ) == SIR_OBJECT_OK && observed.nTypeId == 2 );
	assert( objects.destroy( first ) == SIR_OBJECT_OK );
	assert( objects.resolve( first, &observed ) == SIR_OBJECT_STALE_HANDLE );
	sirObjectHandle_t replacement = {};
	assert( objects.create( 44, 1, &replacement ) == SIR_OBJECT_OK );
	assert( replacement.iSlot == first.iSlot && replacement.nGeneration != first.nGeneration );
	assert( objects.getCount() == 2 );
	objects.shutdown();
	types.shutdown();
	return 0;
}
