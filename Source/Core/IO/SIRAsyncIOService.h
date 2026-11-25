#ifndef __SIR_ASYNCIOSERVICE_H__
#define __SIR_ASYNCIOSERVICE_H__
#include <Core/IO/SIRIOTypes.h>
class SIRAsyncIOService
{
public:
	virtual ~SIRAsyncIOService( void ) noexcept = default;
	virtual sirIOResult_t submit( const sirIORequest_t&, sirIOHandle_t* ) noexcept = 0;
	virtual sirIOResult_t submitBatch( const sirIOBatch_t&, sirIOHandle_t* ) noexcept = 0;
	virtual sirIOResult_t cancel( sirIOHandle_t ) noexcept = 0;
	virtual uint32_t collect( sirIOCompletion_t*, uint32_t ) noexcept = 0;
	virtual sirIOResult_t shutdown( void ) noexcept = 0;
};
#endif
