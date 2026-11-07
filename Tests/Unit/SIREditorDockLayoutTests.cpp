#include <Editor/Widgets/SIREditorDockLayout.h>
#include <assert.h>
#include <stdio.h>

int main( void )
{
	SIREditorDockLayout dock;
	sirEditorDockNode_t nodes[9] = {};
	uint32_t stack[9] = {};
	uint32_t root = UINT32_MAX;
	assert( dock.createPanel( 1, &root ) == SIR_EDITOR_DOCK_NOT_INITIALIZED );
	assert( dock.initialize( nodes, stack, 9 ) == SIR_EDITOR_DOCK_OK );
	assert( dock.createPanel( 1, &root ) == SIR_EDITOR_DOCK_OK && root == 0 );
	assert( dock.split( 0, SIR_EDITOR_DOCK_SPLIT_HORIZONTAL, 300, 2 ) == SIR_EDITOR_DOCK_OK );
	assert( dock.split( 2, SIR_EDITOR_DOCK_SPLIT_VERTICAL, 500, 3 ) == SIR_EDITOR_DOCK_OK );
	assert( dock.split( 0, SIR_EDITOR_DOCK_SPLIT_HORIZONTAL, 500, 4 ) == SIR_EDITOR_DOCK_INVALID_ARGUMENT );
	assert( dock.split( 1, SIR_EDITOR_DOCK_SPLIT_VERTICAL, 500, 2 ) == SIR_EDITOR_DOCK_INVALID_ARGUMENT );
	assert( dock.layout( { 0, 0, 1000, 800 }, 32 ) == SIR_EDITOR_DOCK_OK );
	sirUIRect_t bounds = {};
	assert( dock.getPanelBounds( 1, &bounds ) == SIR_EDITOR_DOCK_OK );
	assert( bounds.iX == 0 && bounds.nWidth == 300 && bounds.nHeight == 800 );
	assert( dock.getPanelBounds( 2, &bounds ) == SIR_EDITOR_DOCK_OK );
	assert( bounds.iX == 300 && bounds.nWidth == 700 && bounds.nHeight == 400 );
	assert( dock.getPanelBounds( 3, &bounds ) == SIR_EDITOR_DOCK_OK );
	assert( bounds.iX == 300 && bounds.iY == 400 && bounds.nWidth == 700 && bounds.nHeight == 400 );
	assert( dock.layout( { 0, 0, 10, 10 }, 32 ) == SIR_EDITOR_DOCK_TOO_SMALL );
	dock.shutdown();
	assert( dock.getPanelBounds( 1, &bounds ) == SIR_EDITOR_DOCK_NOT_INITIALIZED );
	puts( "PASS: retained editor docking algorithm" );
	return 0;
}
