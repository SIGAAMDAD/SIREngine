#ifndef __SIR_COMPRESSIONTYPES_H__
#define __SIR_COMPRESSIONTYPES_H__

#include <Streaming/Pack/SIRPackFormat.h>
#include <Resources/SIRResourceTypes.h>

typedef struct sirCompressionDesc_s
{
	SIRResourceConstBytes source;
	SIRResourceMutableBytes destination;
	SIRResourceMutableBytes scratch;
	uint16_t nCodecVersion;
	uint32_t nDictionaryId;
} sirCompressionDesc_t;

#endif // __SIR_COMPRESSIONTYPES_H__
