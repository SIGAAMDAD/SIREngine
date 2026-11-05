#include <meshoptimizer.h>

#include <stddef.h>

int main( void )
{
	const float szVertices[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const unsigned int szIndices[3] = { 0, 1, 2 };
	unsigned int szRemap[3] = { 0, 0, 0 };

	const size_t nVertices = meshopt_generateVertexRemap( szRemap, szIndices, 3, szVertices, 3, sizeof( float ) * 3 );
	return nVertices == 3 ? 0 : 1;
}
