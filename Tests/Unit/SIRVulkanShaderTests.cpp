/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

#include <stdio.h>

int main( void )
{
	const uint32_t szWords[6] = { 0x07230203U, 0x00010600U, 0U, 6U, 0U, 0x00020011U };
	if ( !sirVulkanValidateSpirv( szWords, sizeof( szWords ), "main" ) ) {
		return 1;
	}
	if ( sirVulkanValidateSpirv( nullptr, sizeof( szWords ), "main" ) ||
	     sirVulkanValidateSpirv( szWords, 19, "main" ) ||
	     sirVulkanValidateSpirv( szWords, 21, "main" ) ||
	     sirVulkanValidateSpirv( szWords, sizeof( szWords ), "" ) ||
	     sirVulkanValidateSpirv( szWords, sizeof( szWords ), nullptr ) ||
	     sirVulkanValidateSpirv( ( ( const uint8_t * )szWords ) + 1, 20, "main" ) ) {
		return 2;
	}
	uint32_t szInvalid[5] = { 0x07230203U, 0x00010600U, 0U, 6U, 0U };
	szInvalid[0] = 0x03022307U;
	if ( sirVulkanValidateSpirv( szInvalid, sizeof( szInvalid ), "main" ) ) {
		return 3;
	}
	szInvalid[0] = 0x07230203U;
	szInvalid[1] = 0x00010700U;
	if ( sirVulkanValidateSpirv( szInvalid, sizeof( szInvalid ), "main" ) ) {
		return 4;
	}
	szInvalid[1] = 0x00000000U;
	if ( sirVulkanValidateSpirv( szInvalid, sizeof( szInvalid ), "main" ) ) {
		return 5;
	}
	szInvalid[1] = 0x00010600U;
	szInvalid[3] = 0U;
	if ( sirVulkanValidateSpirv( szInvalid, sizeof( szInvalid ), "main" ) ) {
		return 6;
	}
	szInvalid[3] = 6U;
	szInvalid[4] = 1U;
	if ( sirVulkanValidateSpirv( szInvalid, sizeof( szInvalid ), "main" ) ) {
		return 7;
	}
	szInvalid[4] = 0U;
	char szEntry[129] = {};
	for ( uint32_t i = 0; i < 128; ++i ) {
		szEntry[i] = 'a';
	}
	if ( sirVulkanValidateSpirv( szInvalid, sizeof( szInvalid ), szEntry ) ) {
		return 8;
	}
	szEntry[127] = '\0';
	if ( !sirVulkanValidateSpirv( szInvalid, sizeof( szInvalid ), szEntry ) ) {
		return 9;
	}
	printf( "Vulkan SPIR-V envelope contracts passed\n" );
	return 0;
}
