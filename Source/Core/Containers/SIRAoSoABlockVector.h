/* SIREngine - MPL-2.0. Blocked two-column array of structures of arrays. */
#ifndef SIR_AOSOA_BLOCK_VECTOR_H
#define SIR_AOSOA_BLOCK_VECTOR_H
#include <Core/Containers/SIRVector.h>
#include <type_traits>

template <typename A, typename B, uint32_t LANES = 16>
class SIRAoSoABlockVector
{
	static_assert( LANES > 0 && LANES <= 256, "AoSoA lane count out of range" );
	static_assert( std::is_default_constructible<A>::value && std::is_default_constructible<B>::value,
	               "Blocked AoSoA columns require default-constructible entries" );
	static_assert( std::is_nothrow_copy_assignable<A>::value && std::is_nothrow_copy_assignable<B>::value,
	               "AoSoA append requires nonthrowing column assignment for atomic element publication" );
	struct alignas( 64 ) Block
	{
		A szFirst[LANES]{};
		B szSecond[LANES]{};
	};
	SIRVector<Block> m_blocks;
	uint32_t m_nSize = 0;

public:
	explicit SIRAoSoABlockVector( const sirContainerAllocator_t *pAllocator ) : m_blocks( pAllocator )
	{
	}
	SIRContainerResult tryEmplaceBack( const A& first, const B& second )
	{
		if ( m_nSize == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		// Stage before growing m_blocks: inputs may alias an existing column.
		A stagedFirst( first );
		B stagedSecond( second );
		if ( m_nSize % LANES == 0 ) {
			const SIRContainerResult eResult = m_blocks.tryEmplaceBack();
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		Block& block = m_blocks[m_nSize / LANES];
		block.szFirst[m_nSize % LANES] = stagedFirst;
		block.szSecond[m_nSize % LANES] = stagedSecond;
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	A *tryGetFirst( uint32_t iIndex ) noexcept
	{
		return iIndex < m_nSize ? &m_blocks[iIndex / LANES].szFirst[iIndex % LANES] : nullptr;
	}
	B *tryGetSecond( uint32_t iIndex ) noexcept
	{
		return iIndex < m_nSize ? &m_blocks[iIndex / LANES].szSecond[iIndex % LANES] : nullptr;
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}
	uint32_t getBlockCount( void ) const noexcept
	{
		return m_blocks.getSize();
	}
	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return m_blocks.getFootprint();
	}
};
#endif
