/* SIREngine MPLv2 - encoding-aware immutable GRDG packed integer projection. */
#ifndef __SIR_GRDGPACKEDINTEGERVIEW_H__
#define __SIR_GRDGPACKEDINTEGERVIEW_H__
#include <Resources/SIRGRDGVectorViews.h>
#include <Core/Containers/SIRPackedIntegerVector.h>

static constexpr uint32_t SIR_GRDG_VECTOR_ENCODING_FIXED_BITS = 1;

class SIRGRDGPackedIntegerView
{
private:
	SIRResourceView m_Source;
	SIRPackedIntegerReadView m_Data;
	uint64_t m_nRevision = 0;

public:
	SIRGRDGPackedIntegerView( void ) noexcept = default;
	bool isValid( void ) const noexcept
	{
		return m_Source.isValid() && m_Source.getRevision() == m_nRevision;
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_Data.getSize();
	}
	SIRResourceResult tryReadAt( uint32_t iIndex, uint64_t *pValue ) const noexcept
	{
		if ( !isValid() ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		return m_Data.tryGet( iIndex, pValue ) ? SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS : SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	static SIRResourceResult tryResolve( const SIRResourceReadScope& rScope, const SIRResourceView& rResource,
	                                     const SIRGRDGVectorReadRequest& rRequest, SIRGRDGPackedIntegerView *pView ) noexcept
	{
		if ( pView == nullptr || !rResource.belongsToScope( rScope ) ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		if ( rRequest.layout.nEncodingId != SIR_GRDG_VECTOR_ENCODING_FIXED_BITS || rRequest.layout.nElementBytes != 8 ||
		     rRequest.layout.nElementCount == 0 ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		SIRResourceInfo info {};
		if ( rResource.getInfo( &info ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS || rRequest.layout.nFormatId != info.m_nFormatId ||
		     rRequest.layout.nSchemaVersion != info.m_nSchemaVersion ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		if ( rRequest.nExpectedRevision != 0 && rRequest.nExpectedRevision != info.m_nRevision ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_STALE_REVISION;
		}
		SIRResourceConstBytes bytes {};
		const SIRResourceResult eResult = rResource.getChunk( rRequest.idChunk, &bytes );
		if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResult;
		}
		SIRPackedIntegerReadView decoded;
		if ( SIRPackedIntegerReadView::tryDecode( bytes.m_pData, bytes.m_nSize, &decoded ) != SIRContainerResult::SIR_CONTAINER_OK ||
		     decoded.getSize() != rRequest.layout.nElementCount ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		pView->m_Source = rResource;
		pView->m_Data = decoded;
		pView->m_nRevision = info.m_nRevision;
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}
};
#endif // __SIR_GRDGPACKEDINTEGERVIEW_H__
