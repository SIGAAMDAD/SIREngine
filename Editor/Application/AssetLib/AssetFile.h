#ifndef __SIRENGINE_ASSET_FILE_H__
#define __SIRENGINE_ASSET_FILE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

namespace SIREngine {
	enum class EAssetType {
		CompiledScript
	};

	class CAssetFile
	{
	public:
		CAssetFile( void );
		~CAssetFile();
	private:
	};
};

#endif