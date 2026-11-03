# Framework allocator implementations are subordinate to the SIR memory page authority.
# The existing SIRMemoryManager.cpp remains owned by Source/Core/CMakeLists.txt.
target_sources(SIRCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRAllocationRouter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRBitmapAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRBuddyAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRCacheAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRCompactBucketAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRFrameAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRFreeListAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRGeneralHeap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRHandleHeap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRHighLowAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRHeapBlock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRJobScratchAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRLinearAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRMediumPageAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRMemoryColdPageStager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRMemoryLayoutPlanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRMemoryPressureCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRPageRunAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRPoolAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRReclaimableSizeClassAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRRingAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRSharedMemoryPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRSizeClassAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRSlabAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRTLSFAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRThreadLocalAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SIRVirtualStackAllocator.cpp
)
