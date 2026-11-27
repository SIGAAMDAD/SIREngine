#ifndef __SIR_EDITORAUTHORINGSESSION_H__
#define __SIR_EDITORAUTHORINGSESSION_H__

#include <Editor/Host/SIREditorObjectInspector.h>
#include <Editor/Host/SIREditorSceneWorkspace.h>
#include <Editor/Host/SIREditorWorkflow.h>
#include <Editor/Scene/SIREditorScalarBinding.h>

class SIRMemorySegment;

// Small, owned composition of the existing authoring services used by the
// graphical vertical slice. It does not introduce a second scene authority.
class SIREditorAuthoringSession
{
public:
	SIREditorAuthoringSession( void ) noexcept;
	SIREditorAuthoringSession( const SIREditorAuthoringSession& ) = delete;
	~SIREditorAuthoringSession();

	bool initialize( SIRMemorySegment *pEditorSegment, const char *pszSource, size_t nSourceBytes ) noexcept;
	void shutdown( void ) noexcept;
	bool isReady( void ) const noexcept { return m_bReady; }
	uint32_t getFixtureNodeCount( void ) const noexcept { return m_nFixtureCount; }
	uint64_t getFixtureNodeId( uint32_t iFixture ) const noexcept;
	uint32_t getPropertyId( void ) const noexcept { return m_nPropertyId; }
	SIREditorSceneWorkspace& getWorkspace( void ) noexcept { return m_Workspace; }
	const SIREditorSceneWorkspace& getWorkspace( void ) const noexcept { return m_Workspace; }
	SIREditorWorkflow& getWorkflow( void ) noexcept { return m_Workflow; }
	SIREditorObjectInspector& getInspector( void ) noexcept { return m_Inspector; }

private:
	SIREditorSceneWorkspace m_Workspace;
	SIREditorWorkflow m_Workflow;
	SIREditorObjectInspector m_Inspector;
	SIRObjectRegistry m_Registry;
	sirObjectTypeDescriptor_t m_Types[8]{};
	sirObjectPropertyDescriptor_t m_Properties[8]{};
	sirEditorScalarBinding_t m_Bindings[8]{};
	sirEditorProperty_t m_Scratch[8]{};
	sirEditorTextEdit_t m_Edits[8]{};
	char m_EncodedScalars[8 * 128]{};
	uint64_t m_FixtureIds[8]{};
	uint32_t m_nFixtureCount{};
	uint32_t m_nPropertyId{};
	bool m_bRegistryReady{};
	bool m_bReady{};
};

#endif
