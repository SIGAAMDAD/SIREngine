#ifndef __SIR_IOBUFFERPOOL_H__
#define __SIR_IOBUFFERPOOL_H__
#include <Core/IO/SIRIOTypes.h>
class SIRIOBufferPool final
{
public:
	SIRIOBufferPool( void ) noexcept;
	sirIOResult_t initialize( const sirIOBufferPoolConfig_t& config ) noexcept;
	sirIOResult_t acquire( uint64_t nBytes, uint32_t nAlignment, sirIOBufferLease_t *pLease ) noexcept;
	sirIOResult_t release( sirIOBufferHandle_t handle ) noexcept;
	uint64_t getCapacity( void ) const noexcept;
	uint64_t getUsedBytes( void ) const noexcept;
	sirIOResult_t shutdown( void ) noexcept;
private:
	uint8_t *m_pStorage; uint64_t m_nCapacity, m_nUsed; sirIOBufferLease_t *m_pSlots; uint32_t m_nSlotCapacity, m_nMaxAlignment; bool m_bInitialized;
};
#endif // __SIR_IOBUFFERPOOL_H__
