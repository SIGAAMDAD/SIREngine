#include "VulkanUtils.h"

using namespace SIREngine::Rendering::Vulkan;

char **GetSupportedExtensions( VkPhysicalDevice hDevice, uint32_t *pSupportedExtensionCount )
{
	VkExtensionProperties *pExtensions;
	static char **ppExtensionList, *pMemory;
	char *pNamePtr;

	vkEnumerateDeviceExtensionProperties( hDevice, NULL, pSupportedExtensionCount, NULL );
	pExtensions = (VkExtensionProperties *)malloc( sizeof( *pExtensions ) * *pSupportedExtensionCount );
	ppExtensionList = (char **)calloc( sizeof( *ppExtensionList ) * *pSupportedExtensionCount, 1 );
	pMemory = (char *)alloca( SIRENGINE_PAD( 72 * *pSupportedExtensionCount, 16 ) );
	vkEnumerateDeviceExtensionProperties( hDevice, NULL, pSupportedExtensionCount, pExtensions );
	
	printf( "Vulkan API Supported Extensions:\n" );
	for ( uint32_t i = 0; i < *pSupportedExtensionCount; i++ ) {
		size_t length = strlen( pExtensions[ i ].extensionName );
		pNamePtr = (char *)SIRENGINE_PAD( (uintptr_t)pMemory + ( 72 * *pSupportedExtensionCount ), 16 );

		strcpy_s( pNamePtr, length + 1, pExtensions[ i ].extensionName );
		ppExtensionList[ *pSupportedExtensionCount ] = pNamePtr;
		printf( "- %s\n", ppExtensionList[ *pSupportedExtensionCount ] );

		( *pSupportedExtensionCount )++;
	}

	free( pExtensions );

	return ppExtensionList;
}