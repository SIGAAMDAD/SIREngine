#include "SIR/Resource/SIRResourceGraph.h"
#include "SIR/Resource/SIRSystemResourceViews.h"

static_assert(SIRGeometryResourceView::RESOURCE_KIND == SIRResourceKind::Geometry, "Kind mismatch");
static_assert(SIRRenderMeshView::RESOURCE_KIND == SIRResourceKind::Geometry, "Projection must retain canonical kind");
static_assert(SIRCollisionMeshView::RESOURCE_KIND == SIRResourceKind::Geometry, "Projection must retain canonical kind");
static_assert(sizeof(SIRResourceHandle) == 16, "Handle format changed");

int main(void)
{
    return 0;
}
