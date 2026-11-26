#include "SIRVulkanProgram.h"

SIRVulkanProgram::SIRVulkanProgram( const SIRSharedPtr<SIRRenderDevice>& device, const sirContainerAllocator_t *pAllocator ) noexcept
    : SIRRenderProgram( device ),
      m_nProgramHash( 0 ),
      m_nStageMask( 0 ),
      m_Shaders( pAllocator )
{
}
