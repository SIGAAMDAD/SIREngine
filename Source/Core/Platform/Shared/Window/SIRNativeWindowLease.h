#ifndef __SIR_NATIVEWINDOWLEASE_H__
#define __SIR_NATIVEWINDOWLEASE_H__
#include <Core/SIRPCH.h>
#include <Core/Platform/Shared/Window/SIRWindowTypes.h>
class SIRWindowSystem;

// The system must outlive the lease. Retire GPU work before resetting it.
class SIRNativeWindowLease final
{
public:
	explicit SIRNativeWindowLease( void ) noexcept;
	explicit SIRNativeWindowLease( const SIRNativeWindowLease& ) = delete;
	explicit SIRNativeWindowLease( SIRNativeWindowLease&& other ) noexcept;
	~SIRNativeWindowLease() noexcept;

	SIRNativeWindowLease& operator=( const SIRNativeWindowLease& ) = delete;
	SIRNativeWindowLease& operator=( SIRNativeWindowLease&& ) = delete;
	[[nodiscard]] bool isValid( void ) const noexcept;
	[[nodiscard]] const sirWindowNativeView_t& getView( void ) const noexcept;
	[[nodiscard]] sirWindowResult reset( void ) noexcept;

private:
	friend class SIRWindowSystem;
	SIRWindowSystem *m_pOwner;
	sirWindowNativeLease_t m_Lease;
	sirWindowNativeView_t m_View;
};
#endif // __SIR_NATIVEWINDOWLEASE_H__
