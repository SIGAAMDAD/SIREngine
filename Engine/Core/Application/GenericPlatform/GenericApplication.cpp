#include "GenericApplication.h"

IGenericApplication::IGenericApplication( const ApplicationInfo_t& appInfo, const CVector<CString>& cmdLine )
    : m_ApplicationInfo( appInfo ), m_CommandLineArgs( std::move( cmdLine ) )
{
}

IGenericApplication::~IGenericApplication()
{
}

void IGenericApplication::Init( void )
{
    //
    // initialize the engine
    //

    g_pRenderContext = IRenderContext::CreateRenderContext();
}