#include <UI/SIRControlTree.h>
#include <assert.h>

int main( void )
{
	sirUIControl_t szControls[16] = {};
	uint32_t szStack[16] = {};
	SIRControlTree ui;
	assert( ui.initialize( szControls, szStack, 16 ) == SIR_UI_OK );
	sirUIHandle_t root = {}, a = {}, b = {}, child = {};
	assert( ui.create( nullptr, { 0, 0, 0, 0 }, SIR_UI_LAYOUT_VERTICAL, false, true, &root ) == SIR_UI_OK );
	assert( ui.create( &root, { 0, 0, 0, 40 }, SIR_UI_LAYOUT_ABSOLUTE, true, true, &a ) == SIR_UI_OK );
	assert( ui.create( &root, { 0, 0, 0, 60 }, SIR_UI_LAYOUT_ABSOLUTE, true, true, &b ) == SIR_UI_OK );
	assert( ui.create( &a, { 5, 5, 40, 20 }, SIR_UI_LAYOUT_ABSOLUTE, true, true, &child ) == SIR_UI_OK );
	assert( ui.layout( { 0, 0, 200, 120 } ) == SIR_UI_OK );
	sirUIRect_t rect = {};
	assert( ui.getBounds( a, &rect ) == SIR_UI_OK && rect.nWidth == 200 && rect.iY == 0 );
	assert( ui.getBounds( b, &rect ) == SIR_UI_OK && rect.iY == 40 && rect.nHeight == 60 );
	assert( ui.getBounds( child, &rect ) == SIR_UI_OK && rect.iX == 5 && rect.iY == 5 );
	sirUIHandle_t hit = {};
	assert( ui.hitTest( 7, 7, &hit ) == SIR_UI_OK && hit.iSlot == child.iSlot );
	assert( ui.hitTest( 50, 70, &hit ) == SIR_UI_OK && hit.iSlot == b.iSlot );
	const uint64_t nPaintBefore = ui.getPaintEpoch();
	assert( ui.dispatch( { SIR_UI_INPUT_POINTER_DOWN, 7, 7, 0 }, &hit ) == SIR_UI_OK );
	assert( ui.getFocus().iSlot == child.iSlot && ui.getPaintEpoch() > nPaintBefore );
	assert( ui.dispatch( { SIR_UI_INPUT_KEY_DOWN, 0, 0, 65 }, &hit ) == SIR_UI_OK && hit.iSlot == child.iSlot );
	assert( ui.setVisible( a, false ) == SIR_UI_OK );
	assert( ui.getFocus().iSlot == UINT32_MAX );
	assert( ui.hitTest( 7, 7, &hit ) == SIR_UI_OK && hit.iSlot == root.iSlot );
	assert( ui.setVisible( a, true ) == SIR_UI_OK );
	assert( ui.remove( a ) == SIR_UI_OK );
	assert( ui.getCount() == 2 );
	assert( ui.getFocus().iSlot == UINT32_MAX );
	assert( ui.getBounds( child, &rect ) == SIR_UI_STALE_HANDLE );
	sirUIHandle_t replacement = {};
	assert( ui.create( &root, { 0, 0, 0, 10 }, SIR_UI_LAYOUT_ABSOLUTE, true, false, &replacement ) == SIR_UI_OK );
	assert( replacement.iSlot == a.iSlot && replacement.nGeneration != a.nGeneration );
	assert( ui.layout( { 0, 0, 200, 120 } ) == SIR_UI_OK );
	assert( ui.remove( root ) == SIR_UI_OK );
	assert( ui.getCount() == 0 );
	ui.shutdown();
	return 0;
}
