# Native bounded logical VFS path validation.
add_executable(SIRVFSPathTests SIRVFSPathTests.cpp)
target_link_libraries(SIRVFSPathTests PRIVATE SIR::Core)
add_test(NAME core.vfs.path-normalization COMMAND SIRVFSPathTests)
sir_configure_owned_target(SIRVFSPathTests)

# Bounded VFS mount registry, overlay precedence, and lifetime leases.
add_executable(SIRVFSMountTableTests SIRVFSMountTableTests.cpp)
target_link_libraries(SIRVFSMountTableTests PRIVATE SIR::Core)
add_test(NAME core.vfs.mount-registry COMMAND SIRVFSMountTableTests)
sir_configure_owned_target(SIRVFSMountTableTests)

# VFS metadata-only overlay selection and generation-pinned winner lease.
add_executable(SIRVFSOverlayResolverTests SIRVFSOverlayResolverTests.cpp)
target_link_libraries(SIRVFSOverlayResolverTests PRIVATE SIR::Core)
add_test(NAME core.vfs.overlay-resolver COMMAND SIRVFSOverlayResolverTests)
sir_configure_owned_target(SIRVFSOverlayResolverTests)

# VFS immutable logical file identity, source pins and deferred close.
add_executable(SIRVFSFileHandleTableTests SIRVFSFileHandleTableTests.cpp)
target_link_libraries(SIRVFSFileHandleTableTests PRIVATE SIR::Core)
add_test(NAME core.vfs.file-handle-lifetime COMMAND SIRVFSFileHandleTableTests)
sir_configure_owned_target(SIRVFSFileHandleTableTests)

# Root-bound native directory opens: traversal/reparse denial and held handles.
add_executable(SIRPlatformRootDirectoryTests SIRPlatformRootDirectoryTests.cpp)
target_link_libraries(SIRPlatformRootDirectoryTests PRIVATE SIR::Core)
add_test(NAME core.platform.rooted-directory COMMAND SIRPlatformRootDirectoryTests)
sir_configure_owned_target(SIRPlatformRootDirectoryTests)

# Generation-pinned VFS directory source to native platform read integration.
add_executable(SIRVFSDirectorySourceTests SIRVFSDirectorySourceTests.cpp)
target_link_libraries(SIRVFSDirectorySourceTests PRIVATE SIR::Core)
add_test(NAME core.vfs.directory-source COMMAND SIRVFSDirectorySourceTests)
sir_configure_owned_target(SIRVFSDirectorySourceTests)

