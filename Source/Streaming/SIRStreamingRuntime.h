#ifndef __SIR_STREAMINGRUNTIME_H__
#define __SIR_STREAMINGRUNTIME_H__
#include <Streaming/SIRResourceStreamingCoordinator.h>
typedef struct sirStreamingRuntimeConfig_s
{
	SIRResourceStreamingCoordinator *pResources;
	SIRStreamingCoordinator *pCoordinator;
} sirStreamingRuntimeConfig_t;
class SIRStreamingRuntime final
{
public:
	SIRStreamingRuntime( void ) noexcept;
	SIRStreamingRuntime( const SIRStreamingRuntime& ) = delete;
	SIRStreamingRuntime& operator=( const SIRStreamingRuntime& ) = delete;
	sirStreamingResult_t initialize( const sirStreamingRuntimeConfig_t& ) noexcept;
	sirStreamingResult_t tick( const sirStreamingBudget_t& ) noexcept;
	sirStreamingResult_t shutdown( void ) noexcept;
private: SIRResourceStreamingCoordinator *m_pResources; SIRStreamingCoordinator *m_pCoordinator; bool m_bInitialized;
};
#endif
