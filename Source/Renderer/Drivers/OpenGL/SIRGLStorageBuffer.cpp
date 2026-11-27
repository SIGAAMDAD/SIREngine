#include "SIRGLStorageBuffer.h"
#include "SIRGLDevice.h"

static GLenum toBufferTarget( SIRRenderBufferUsage eUsage ) noexcept
{
	if ( ( eUsage & SIRRenderBufferUsage::Vertex ) != SIRRenderBufferUsage::None ) {
		return GL_ARRAY_BUFFER;
	}
	if ( ( eUsage & SIRRenderBufferUsage::Index ) != SIRRenderBufferUsage::None ) {
		return GL_ELEMENT_ARRAY_BUFFER;
	}
	if ( ( eUsage & SIRRenderBufferUsage::Texel ) != SIRRenderBufferUsage::None ) {
		return GL_TEXTURE_BUFFER;
	}
	if ( ( eUsage & SIRRenderBufferUsage::Uniform ) != SIRRenderBufferUsage::None ) {
		return GL_UNIFORM_BUFFER;
	}
	if ( ( eUsage & SIRRenderBufferUsage::Storage ) != SIRRenderBufferUsage::None ) {
		return GL_SHADER_STORAGE_BUFFER;
	}
	if ( ( eUsage & SIRRenderBufferUsage::QueryResults ) != SIRRenderBufferUsage::None ) {
		return GL_QUERY_BUFFER;
	}
	if ( ( eUsage & SIRRenderBufferUsage::Indirect ) != SIRRenderBufferUsage::None ) {
		return GL_DRAW_INDIRECT_BUFFER;
	}
	if ( ( eUsage & SIRRenderBufferUsage::CopyDestination ) != SIRRenderBufferUsage::None ) {
		return GL_COPY_WRITE_BUFFER;
	}
	if ( ( eUsage & SIRRenderBufferUsage::CopySource ) != SIRRenderBufferUsage::None ) {
		return GL_COPY_READ_BUFFER;
	}
	return GL_INVALID_ENUM;
}

static GLbitfield getStorageFlags( SIRRenderMemoryClass eMemory, bool bPersistent ) noexcept
{
	GLbitfield flags = GL_DYNAMIC_STORAGE_BIT;
	if ( eMemory == SIRRenderMemoryClass::Readback || eMemory == SIRRenderMemoryClass::Unified ) {
		flags |= GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
	}
	if ( eMemory == SIRRenderMemoryClass::Upload ) {
		flags |= GL_MAP_WRITE_BIT;
	}
	if ( bPersistent ) {
		flags |= GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	}
	return flags;
}

static GLbitfield getMapFlags( SIRRenderMemoryClass eMemory, bool bPersistent ) noexcept
{
	GLbitfield flags = GL_MAP_WRITE_BIT;
	if ( eMemory == SIRRenderMemoryClass::Readback || eMemory == SIRRenderMemoryClass::Unified ) {
		flags |= GL_MAP_READ_BIT;
	}
	if ( bPersistent ) {
		flags |= GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	}
	return flags;
}

static bool validRange( uint64_t nBufferSize, uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept
{
	return nOffsetBytes <= nBufferSize && nSizeBytes <= nBufferSize - nOffsetBytes;
}

SIRGLStorageBuffer::SIRGLStorageBuffer( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderBufferDesc& desc ) noexcept
    : SIRRenderStorageBuffer( device, desc ),
      m_hBufferID( 0 ),
      m_pMappedBuffer( nullptr )
{
	if ( device.get() == nullptr || device->getBackend() != SIRRenderBackend::OpenGL || m_nSize == 0 || m_eUsage == SIRRenderBufferUsage::None )
		return;
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	if ( !glDevice.hasExtension( "GL_ARB_vertex_buffer_object" ) )
		return;
	const GLenum target = toBufferTarget( m_eUsage );
	if ( target == GL_INVALID_ENUM )
		return;
	const bool bDirectStateAccess = glDevice.hasExtension( "GL_ARB_direct_state_access" );
	const bool bBufferStorage = glDevice.hasExtension( "GL_ARB_buffer_storage" );
	if ( bDirectStateAccess ) {
		glDevice.createBuffers( 1, &m_hBufferID );
		if ( m_hBufferID != 0 ) {
			if ( bBufferStorage )
				glDevice.namedBufferStorage( m_hBufferID, static_cast<GLsizeiptr>( m_nSize ), nullptr, getStorageFlags( m_eMemoryType, m_bPersistentlyMapped ) );
			else
				glDevice.namedBufferData( m_hBufferID, static_cast<GLsizeiptr>( m_nSize ), nullptr, GL_DYNAMIC_DRAW );
		}
	} else {
		glDevice.genBuffers( 1, &m_hBufferID );
		if ( m_hBufferID != 0 ) {
			glDevice.bindBuffer( target, m_hBufferID );
			if ( bBufferStorage ) {
				glDevice.bufferStorage( target, static_cast<GLsizeiptr>( m_nSize ), nullptr, getStorageFlags( m_eMemoryType, m_bPersistentlyMapped ) );
			} else {
				glDevice.bufferData( target, static_cast<GLsizeiptr>( m_nSize ), nullptr, GL_DYNAMIC_DRAW );
			}
			glDevice.bindBuffer( target, 0 );
		}
	}
	if ( m_hBufferID != 0 )
		m_nCommittedBytes = m_nSize;
}

SIRGLStorageBuffer::~SIRGLStorageBuffer() noexcept
{
	if ( m_hBufferID == 0 ) {
		return;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	if ( m_pMappedBuffer != nullptr ) {
		static_cast<void>( unmap() );
	}
	if ( glDevice.hasExtension( "GL_ARB_vertex_buffer_object" ) && glDevice.isBuffer( m_hBufferID ) ) {
		glDevice.deleteBuffers( 1, &m_hBufferID );
	}
}

SIRRenderResult SIRGLStorageBuffer::map( uint64_t nOffsetBytes, uint64_t nSizeBytes, SIRRenderMappedBuffer *pMapping ) noexcept
{
	if ( !validRange( m_nSize, nOffsetBytes, nSizeBytes ) ) {
		return SIRRenderResult::InvalidArgument;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	if ( !glDevice.hasExtension( "GL_ARB_vertex_buffer_object" ) || !glDevice.hasExtension( "GL_ARB_map_buffer_range" ) ) {
		return SIRRenderResult::Unsupported;
	}

	void *pMapped = nullptr;
	if ( glDevice.hasExtension( "GL_ARB_direct_state_access" ) ) {
		pMapped = glDevice.mapNamedBufferRange(
		    m_hBufferID, static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ), getMapFlags( m_eMemoryType, m_bPersistentlyMapped ) );
	} else {
		glDevice.bindBuffer( toBufferTarget( m_eUsage ), m_hBufferID );
		pMapped = glDevice.mapBufferRange(
		    toBufferTarget( m_eUsage ), static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ), getMapFlags( m_eMemoryType, m_bPersistentlyMapped ) );
	}
	if ( pMapped == nullptr ) {
		return SIRRenderResult::DriverError;
	}

	m_pMappedBuffer = pMapped;
	pMapping->pMapped = pMapped;
	pMapping->nOffsetBytes = nOffsetBytes;
	pMapping->nSizeBytes = nSizeBytes;
	pMapping->nMapToken = reinterpret_cast<uintptr_t>( pMapped );

	return SIRRenderResult::Success;
}

SIRRenderResult SIRGLStorageBuffer::unmap( void ) noexcept
{
	if ( m_pMappedBuffer == nullptr ) {
		return SIRRenderResult::Unsupported;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	const bool bUnmapped = glDevice.hasExtension( "GL_ARB_direct_state_access" ) ? glDevice.unmapNamedBuffer( m_hBufferID ) : glDevice.unmapBuffer( toBufferTarget( m_eUsage ) );
	m_pMappedBuffer = nullptr;

	return bUnmapped ? SIRRenderResult::Success : SIRRenderResult::DriverError;
}

SIRRenderResult SIRGLStorageBuffer::flush( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept
{
	if ( m_pMappedBuffer == nullptr ) {
		return SIRRenderResult::Unsupported;
	}
	if ( !validRange( m_nSize, nOffsetBytes, nSizeBytes ) ) {
		return SIRRenderResult::InvalidArgument;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	if ( glDevice.hasExtension( "GL_ARB_direct_state_access" ) ) {
		glDevice.flushMappedNamedBufferRange( m_hBufferID, static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ) );
	} else {
		glDevice.flushMappedBufferRange( toBufferTarget( m_eUsage ), static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ) );
	}

	return SIRRenderResult::Success;
}

SIRRenderResult SIRGLStorageBuffer::invalidate( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept
{
	if ( !validRange( m_nSize, nOffsetBytes, nSizeBytes ) ) {
		return SIRRenderResult::InvalidArgument;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	if ( !glDevice.atLeast( 4, 3 ) && !glDevice.hasExtension( "GL_ARB_invalidate_subdata" ) ) {
		return SIRRenderResult::Unsupported;
	}

	glDevice.invalidateBufferSubData( m_hBufferID, static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ) );
	return SIRRenderResult::Success;
}

uint64_t SIRGLStorageBuffer::getDeviceAddress( void ) const noexcept
{
	return 0;
}

SIRRenderResult SIRGLStorageBuffer::updateBufferRange( uint64_t nOffsetBytes, uint64_t nSizeBytes, const SIRSpan<uint8_t>& memory ) noexcept
{
	if ( !validRange( m_nSize, nOffsetBytes, nSizeBytes ) ) {
		return SIRRenderResult::InvalidArgument;
	}
	if ( m_hBufferID == 0 ) {
		return SIRRenderResult::InvalidHandle;
	}
	if ( m_pMappedBuffer != nullptr ) {
		return SIRRenderResult::Busy;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	if ( glDevice.hasExtension( "GL_ARB_direct_state_access" ) ) {
		glDevice.namedBufferSubData( m_hBufferID, static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ), memory.getData() );
	} else {
		const GLenum target = toBufferTarget( m_eUsage );
		glDevice.bindBuffer( target, m_hBufferID );
		glDevice.bufferSubData( target, static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ), memory.getData() );
		glDevice.bindBuffer( target, 0 );
	}

	return SIRRenderResult::Success;
}

SIRRenderResult SIRGLStorageBuffer::copyBufferRange( SIRRenderStorageBuffer& copyTarget, uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept
{
	if ( !validRange( m_nSize, nOffsetBytes, nSizeBytes ) ) {
		return SIRRenderResult::InvalidArgument;
	}

	SIRGLStorageBuffer *pTarget = dynamic_cast<SIRGLStorageBuffer *>( &copyTarget );
	if ( pTarget == nullptr || !validRange( pTarget->m_nSize, nOffsetBytes, nSizeBytes ) || pTarget->m_hBufferID == 0 ) {
		return SIRRenderResult::InvalidHandle;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	if ( glDevice.hasExtension( "GL_ARB_direct_state_access" ) ) {
		glDevice.copyNamedBufferSubData(
		    m_hBufferID, pTarget->m_hBufferID, static_cast<GLintptr>( nOffsetBytes ), static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ) );
	} else {
		glDevice.bindBuffer( GL_COPY_READ_BUFFER, m_hBufferID );
		glDevice.bindBuffer( GL_COPY_WRITE_BUFFER, pTarget->m_hBufferID );
		glDevice.copyBufferSubData(
		    GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, static_cast<GLintptr>( nOffsetBytes ), static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ) );
		glDevice.bindBuffer( GL_COPY_READ_BUFFER, 0 );
		glDevice.bindBuffer( GL_COPY_WRITE_BUFFER, 0 );
	}

	return SIRRenderResult::Success;
}

SIRRenderResult SIRGLStorageBuffer::clearBufferRange( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept
{
	if ( !validRange( m_nSize, nOffsetBytes, nSizeBytes ) ) {
		return SIRRenderResult::InvalidArgument;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	const uint8_t zero = 0;
	if ( glDevice.hasExtension( "GL_ARB_direct_state_access" ) ) {
		glDevice.clearNamedBufferSubData(
		    m_hBufferID, GL_R8UI, static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ), GL_RED_INTEGER, GL_UNSIGNED_BYTE, &zero );
	} else if ( glDevice.atLeast( 4, 3 ) || glDevice.hasExtension( "GL_ARB_clear_buffer_object" ) ) {
		const GLenum target = toBufferTarget( m_eUsage );
		glDevice.bindBuffer( target, m_hBufferID );
		glDevice.clearBufferSubData( target, GL_R8UI, static_cast<GLintptr>( nOffsetBytes ), static_cast<GLsizeiptr>( nSizeBytes ), GL_RED_INTEGER, GL_UNSIGNED_BYTE, &zero );
		glDevice.bindBuffer( target, 0 );
	} else {
		return SIRRenderResult::Unsupported;
	}

	return SIRRenderResult::Success;
}

SIRRenderResult SIRGLStorageBuffer::clear( void ) noexcept
{
	return clearBufferRange( 0, m_nSize );
}

SIRRenderResult SIRGLStorageBuffer::bind( void ) const noexcept
{
	if ( m_hBufferID == 0 ) {
		return SIRRenderResult::InvalidHandle;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	const GLenum target = toBufferTarget( m_eUsage );

	if ( target == GL_INVALID_ENUM || !glDevice.hasExtension( "GL_ARB_vertex_buffer_object" ) ) {
		return SIRRenderResult::Unsupported;
	}
	glDevice.bindBuffer( target, m_hBufferID );

	return SIRRenderResult::Success;
}

SIRRenderResult SIRGLStorageBuffer::unbind( void ) const noexcept
{
	if ( m_hBufferID == 0 ) {
		return SIRRenderResult::InvalidHandle;
	}

	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	const GLenum target = toBufferTarget( m_eUsage );

	if ( target == GL_INVALID_ENUM || !glDevice.hasExtension( "GL_ARB_vertex_buffer_object" ) ) {
		return SIRRenderResult::Unsupported;
	}
	glDevice.bindBuffer( target, 0 );

	return SIRRenderResult::Success;
}

bool SIRGLStorageBuffer::isValid( void ) const noexcept
{
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
	return m_hBufferID != 0 && glDevice.hasExtension( "GL_ARB_vertex_buffer_object" ) && glDevice.isBuffer( m_hBufferID );
}
