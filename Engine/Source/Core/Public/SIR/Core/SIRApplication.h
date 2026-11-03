#ifndef SIR_CORE_APPLICATION_H
#define SIR_CORE_APPLICATION_H

#include <stdint.h>

namespace sir
{
typedef enum class SIRAppState : uint8_t
{
	Uninitialized = 0,
	Ready = 1,
	Stopped = 2
} SIRAppState;

class SIRApplication
{
private:
	SIRAppState m_eState;
	uint64_t m_nTicks;

public:
	SIRApplication( void ) noexcept;
	bool initialize( void ) noexcept;
	bool tick( void ) noexcept;
	void shutdown( void ) noexcept;
	SIRAppState getState( void ) const noexcept;
	uint64_t getTickCount( void ) const noexcept;
};
}

#endif
