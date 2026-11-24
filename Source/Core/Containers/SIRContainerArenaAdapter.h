/* SIREngine MPLv2 - explicit linear arena adapter for owning containers. */
#ifndef __SIR_CONTAINERARENAADAPTER_H__
#define __SIR_CONTAINERARENAADAPTER_H__
#include <Core/SIRPCH.h>
#include <Core/Containers/SIRContainerResult.h>
#include <Core/Memory/Allocators/SIRLinearAllocator.h>
#include <Core/Memory/SIRMemorySegment.h>

// Individual deallocations are intentionally no-ops. Reset/rewind is only
// legal after ALL bound vectors have destroyed their live elements.
class SIRContainerArenaAdapter
{
private:
	SIRLinearAllocator *m_pArena = nullptr;
	sirContainerAllocator_t m_Allocator {};

	static void *allocate( void *pContext, size_t nBytes, size_t nAlignment ) noexcept
	{
		SIRContainerArenaAdapter *pAdapter = static_cast<SIRContainerArenaAdapter *>(pContext);
		return pAdapter->m_pArena != nullptr ? pAdapter->m_pArena->alloc( nBytes, nAlignment ) : nullptr;
	}
	static void deallocate( void *, void *, size_t, size_t ) noexcept
	{
	}

public:
	SIRContainerArenaAdapter( void ) noexcept = default;
	SIRContainerArenaAdapter( const SIRContainerArenaAdapter& ) = delete;
	SIRContainerArenaAdapter& operator=( const SIRContainerArenaAdapter& ) = delete;
	bool init( SIRLinearAllocator *pArena, uint32_t nOwnerTag ) noexcept
	{
		if ( m_pArena != nullptr || pArena == nullptr || pArena->getSegment() == nullptr ) {
			return false;
		}
		m_pArena = pArena;
		m_Allocator = { this, allocate, deallocate, static_cast<uint32_t>(pArena->getSegment()->getDomain()), nOwnerTag };
		return true;
	}
	const sirContainerAllocator_t *getAllocator( void ) const noexcept
	{
		return m_pArena != nullptr ? &m_Allocator : nullptr;
	}
};
#endif // __SIR_CONTAINERARENAADAPTER_H__
