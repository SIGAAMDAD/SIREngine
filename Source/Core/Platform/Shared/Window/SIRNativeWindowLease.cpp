#include <Core/Platform/Shared/Window/SIRNativeWindowLease.h>
#include <Core/Platform/Shared/Window/SIRWindowSystem.h>
SIRNativeWindowLease::SIRNativeWindowLease( void ) noexcept : m_pOwner( nullptr ), m_Lease{}, m_View{} {}
SIRNativeWindowLease::SIRNativeWindowLease( SIRNativeWindowLease&& other ) noexcept : m_pOwner( other.m_pOwner ), m_Lease( other.m_Lease ), m_View( other.m_View )
{
 other.m_pOwner = nullptr;
 other.m_Lease = {};
 other.m_View = {};
}
SIRNativeWindowLease::~SIRNativeWindowLease() noexcept { static_cast<void>( reset() ); }
bool SIRNativeWindowLease::isValid( void ) const noexcept { return m_pOwner != nullptr; }
const sirWindowNativeView_t& SIRNativeWindowLease::getView( void ) const noexcept { return m_View; }
sirWindowResult SIRNativeWindowLease::reset( void ) noexcept
{
 if ( m_pOwner == nullptr ) { return sirWindowResult::SIR_WINDOW_OK; }
 const sirWindowResult eResult = m_pOwner->releaseNativeLease( m_Lease );
 if ( eResult == sirWindowResult::SIR_WINDOW_OK ) { m_pOwner = nullptr; m_Lease = {}; m_View = {}; }
 return eResult;
}
