#include "DrawBuffer.h"

using namespace SIREngine::RenderLib;

void CDrawBuffer::AddPolyList( const Poly_t *pPolyList, uint64_t nPolyCount )
{

}

void CDrawBuffer::NewScene( const SceneInfo_t& sceneData )
{
	m_SceneStack.push( sceneData );
	m_pCurrentScene = &m_SceneStack.top();
}

void CDrawBuffer::FinishScene( void )
{
	m_nRenderPassCount++;
}

void CDrawBuffer::Reset( void )
{
	size_t i;

	m_nRenderPassCount = 0;
	for ( i = 0; i < m_SceneStack.size(); i++ ) {
		m_SceneStack.pop();
	}
}

uint64_t CDrawBuffer::GetSceneCount( void ) const
{
	return m_SceneStack.size();
}