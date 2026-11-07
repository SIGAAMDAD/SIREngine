// Pure C++17 validation of the experimental P31a push-constant/slot ABI.
#include "rendering/rd/vt/core/terrain_3d_vt_single_writer.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include <assert.h>
#include <string.h>

int main()
{
	t3dVTFeedbackGridLayout_t layout{};
	assert( t3d_vt_feedback_grid_layout( 150000u, layout ) );
	t3dVTSingleWriterParams_t params{};
	assert( t3d_vt_single_writer_params( layout, 1920u, 1080u, 3u, 12000u, params ) );
	assert( params.m_nGridWidth == layout.m_nWidth );
	assert( params.m_nGridHeight == layout.m_nHeight );
	assert( params.m_nViewportWidth == 1920u && params.m_nViewportHeight == 1080u );
	assert( params.m_nSlot == 3u && params.m_nPageCount == 12000u );
	assert( params.m_nSlotStrideWords == layout.m_nSlotBytes / sizeof( uint32_t ) );
	t3dVTSingleWriterParams_t empty{};
	assert( !t3d_vt_single_writer_params( layout, 0u, 1080u, 3u, 12000u, empty ) );
	assert( !t3d_vt_single_writer_params( layout, 1920u, 1080u, 4u, 12000u, empty ) );
	assert( !t3d_vt_single_writer_params( layout, 1920u, 1080u, 3u, 0u, empty ) );
	assert( !t3d_vt_single_writer_params( layout, 65536u, 1080u, 3u, 12000u, empty ) );
	return 0;
}
