// CS04 read-only, demand-paged sidecar file backing. The OS page cache owns
// physical storage; opening a large .t3sc does not allocate its full payload.
#ifndef TERRAIN3D_COMPOSED_SURFACE_MAPPED_FILE_H
#define TERRAIN3D_COMPOSED_SURFACE_MAPPED_FILE_H
#include "rendering/rd/vt/runtime/terrain_3d_composed_surface_cache.h"

class Terrain3DComposedSurfaceMappedFile
{
public:
	Terrain3DComposedSurfaceMappedFile() = default;
	~Terrain3DComposedSurfaceMappedFile()
	{
		closeCold();
	}
	Terrain3DComposedSurfaceMappedFile( const Terrain3DComposedSurfaceMappedFile& ) = delete;
	Terrain3DComposedSurfaceMappedFile& operator=( const Terrain3DComposedSurfaceMappedFile& ) = delete;
	bool openCold( const char *pUtf8Path, uint64_t nExpectedFingerprint );
	void closeCold();
	const t3dComposedSurfaceSidecarView_t& view() const
	{
		return m_view;
	}

private:
	void *m_pMapping = nullptr;
	uint64_t m_nBytes = 0u;
	t3dComposedSurfaceSidecarView_t m_view{};
#if defined( _WIN32 )
	void *m_pFile = nullptr;
	void *m_pFileMapping = nullptr;
#endif
};
#endif
