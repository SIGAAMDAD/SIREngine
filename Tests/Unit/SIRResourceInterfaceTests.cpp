#include <Resources/SIRResourceGraph.h>
#include <Resources/SIRResourceViews.h>
#include <Renderer/SIRRenderResourceViews.h>
#include <Physics/SIRPhysicsResourceViews.h>

static_assert( SIRGeometryResourceView::RESOURCE_KIND == SIRResourceKind::SIR_RESOURCE_KIND_GEOMETRY, "Kind mismatch" );
static_assert( SIRRenderMeshView::RESOURCE_KIND == SIRResourceKind::SIR_RESOURCE_KIND_GEOMETRY, "Projection must retain canonical kind" );
static_assert( SIRCollisionMeshView::RESOURCE_KIND == SIRResourceKind::SIR_RESOURCE_KIND_GEOMETRY, "Projection must retain canonical kind" );
static_assert( sizeof( SIRResourceHandle ) == 16, "Handle format changed" );
