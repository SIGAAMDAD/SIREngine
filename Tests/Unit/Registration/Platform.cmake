# Core-owned file, mapped-file, and anonymous-pipe services.
foreach(sirTest IN ITEMS SIRPlatformFileTests SIRPlatformFileMappingTests SIRPlatformPipeTests)
    add_executable(${sirTest} ${sirTest}.cpp)
    target_link_libraries(${sirTest} PRIVATE SIR::Core)
    add_test(NAME core.platform.${sirTest} COMMAND ${sirTest})
    sir_configure_owned_target(${sirTest})
endforeach()

add_executable(SIRSemaphoreTests SIRSemaphoreTests.cpp)
target_link_libraries(SIRSemaphoreTests PRIVATE SIR::Core)
add_test(NAME core.platform.semaphore COMMAND SIRSemaphoreTests)
sir_configure_owned_target(SIRSemaphoreTests)

# Platform process, filesystem and current-thread controls.
add_executable(SIRChildProcessTests SIRChildProcessTests.cpp)
target_link_libraries(SIRChildProcessTests PRIVATE SIR::Core)
add_test(NAME core.platform.child-process COMMAND SIRChildProcessTests)
sir_configure_owned_target(SIRChildProcessTests)

add_executable(SIRPlatformFileSystemTests SIRPlatformFileSystemTests.cpp)
target_link_libraries(SIRPlatformFileSystemTests PRIVATE SIR::Core)
add_test(NAME core.platform.filesystem COMMAND SIRPlatformFileSystemTests)
sir_configure_owned_target(SIRPlatformFileSystemTests)

add_executable(SIRPlatformThreadControlTests SIRPlatformThreadControlTests.cpp)
target_link_libraries(SIRPlatformThreadControlTests PRIVATE SIR::Core)
add_test(NAME core.platform.thread-control COMMAND SIRPlatformThreadControlTests)
sir_configure_owned_target(SIRPlatformThreadControlTests)


# Extended cross-platform I/O regression matrix, including descriptor and
# handle lifecycle, concurrent positional writes, mapping, pipe and IPC tests.
add_executable(SIRPlatformIORegressionTests SIRPlatformIORegressionTests.cpp)
target_link_libraries(SIRPlatformIORegressionTests PRIVATE SIR::Core)
add_test(NAME core.platform.io.regression COMMAND SIRPlatformIORegressionTests)
set_tests_properties(core.platform.io.regression PROPERTIES TIMEOUT 120)
sir_configure_owned_target(SIRPlatformIORegressionTests)

# Production Linux syscalls are intercepted at the final test link boundary.
# Fault injection does not modify the shipping platform implementation.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRPlatformIOFaultInjectionTests SIRPlatformIOFaultInjectionTests.cpp)
    target_link_libraries(SIRPlatformIOFaultInjectionTests PRIVATE SIR::Core)
    foreach(sirWrappedCall IN ITEMS open pread pwrite write mmap msync fstat stat fsync ftruncate close sysconf munmap pipe2 sem_wait sem_timedwait posix_spawnp)
        target_link_options(SIRPlatformIOFaultInjectionTests PRIVATE "LINKER:--wrap=${sirWrappedCall}")
    endforeach()
    add_test(NAME core.platform.io.fault-injection COMMAND SIRPlatformIOFaultInjectionTests)
    set_tests_properties(core.platform.io.fault-injection PROPERTIES TIMEOUT 60)
    sir_configure_owned_target(SIRPlatformIOFaultInjectionTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRGRDGDeathTests SIRGRDGDeathTests.cpp)
    target_link_libraries(SIRGRDGDeathTests PRIVATE SIR::Resources)
    target_link_options(SIRGRDGDeathTests PRIVATE "LINKER:--wrap=abort")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_FLAGS MATCHES "--coverage")
        target_compile_definitions(SIRGRDGDeathTests PRIVATE SIR_GRDG_GCOV_ENABLED=1)
    endif()
    add_test(NAME resources.lifetime.death COMMAND SIRGRDGDeathTests)
    sir_configure_owned_target(SIRGRDGDeathTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRTaskSchedulerFaultInjectionTests SIRTaskSchedulerFaultInjectionTests.cpp)
    target_link_libraries(SIRTaskSchedulerFaultInjectionTests PRIVATE SIR::Core)
    target_link_options(SIRTaskSchedulerFaultInjectionTests PRIVATE "LINKER:--wrap=pthread_create")
    add_test(NAME core.jobs.thread-fault-injection COMMAND SIRTaskSchedulerFaultInjectionTests)
    sir_configure_owned_target(SIRTaskSchedulerFaultInjectionTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRPlatformClockProcessFaultTests SIRPlatformClockProcessFaultTests.cpp)
    target_link_libraries(SIRPlatformClockProcessFaultTests PRIVATE SIR::Core)
    foreach(sirWrappedCall IN ITEMS clock_gettime sysconf nanosleep)
        target_link_options(SIRPlatformClockProcessFaultTests PRIVATE "LINKER:--wrap=${sirWrappedCall}")
    endforeach()
    add_test(NAME core.platform.clock-process-faults COMMAND SIRPlatformClockProcessFaultTests)
    sir_configure_owned_target(SIRPlatformClockProcessFaultTests)
endif()


# The original OpenGL/WGL hidden native HWND lifecycle now belongs to Core/Platform.
if(WIN32)
    add_executable(SIRWGLBootstrapWindowTests SIRWGLBootstrapWindowTests.cpp)
    target_link_libraries(SIRWGLBootstrapWindowTests PRIVATE SIR::Core)
    add_test(NAME core.platform.window.wgl-bootstrap COMMAND SIRWGLBootstrapWindowTests)
    set_tests_properties(core.platform.window.wgl-bootstrap PROPERTIES LABELS "core;platform;window;wgl;integration" TIMEOUT 60)
    sir_configure_owned_target(SIRWGLBootstrapWindowTests)
endif()

# Window system registry and headless negative-path acceptance.
add_executable(SIRWindowSystemTests SIRWindowSystemTests.cpp)
target_link_libraries(SIRWindowSystemTests PRIVATE SIR::Core)
add_test(NAME core.platform.window.system COMMAND SIRWindowSystemTests)
set_tests_properties(core.platform.window.system PROPERTIES LABELS "core;platform;window" TIMEOUT 60)
sir_configure_owned_target(SIRWindowSystemTests)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SIR_WITH_X11_INPUT)
    find_package(X11 QUIET)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SIR_WITH_X11_INPUT AND X11_FOUND)
    find_program(SIR_WINDOW_XVFB_RUN NAMES xvfb-run)
    if(SIR_WINDOW_XVFB_RUN)
        add_executable(SIRWindowX11DriverTests SIRWindowX11DriverTests.cpp)
        target_link_libraries(SIRWindowX11DriverTests PRIVATE SIR::Core X11::X11)
        add_test(NAME core.platform.window.x11-native COMMAND "${SIR_WINDOW_XVFB_RUN}" -a "$<TARGET_FILE:SIRWindowX11DriverTests>")
        set_tests_properties(core.platform.window.x11-native PROPERTIES LABELS "core;platform;window;x11;integration" TIMEOUT 60)
        sir_configure_owned_target(SIRWindowX11DriverTests)
    endif()
endif()

# Renderer-side pinned native window lease adapter.
add_executable(SIRWindowSurfaceAdapterTests SIRWindowSurfaceAdapterTests.cpp)
target_link_libraries(SIRWindowSurfaceAdapterTests PRIVATE SIR::Graphics)
add_test(NAME core.platform.window.surface-bridge COMMAND SIRWindowSurfaceAdapterTests)
set_tests_properties(core.platform.window.surface-bridge PROPERTIES LABELS "core;platform;window;renderer" TIMEOUT 60)
sir_configure_owned_target(SIRWindowSurfaceAdapterTests)

# One native event pump routes X11 keyboard/text events to the input backend.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SIR_WITH_X11_INPUT AND X11_FOUND)
    find_program(SIR_INPUT_BRIDGE_XVFB NAMES xvfb-run)
    if(SIR_INPUT_BRIDGE_XVFB)
        add_executable(SIRX11InputBridgeTests SIRX11InputBridgeTests.cpp)
        target_link_libraries(SIRX11InputBridgeTests PRIVATE SIR::Core X11::X11)
        add_test(NAME core.platform.window.x11-input-bridge COMMAND "${SIR_INPUT_BRIDGE_XVFB}" -a "$<TARGET_FILE:SIRX11InputBridgeTests>")
        set_tests_properties(core.platform.window.x11-input-bridge PROPERTIES LABELS "core;platform;input;x11;integration" TIMEOUT 60)
        sir_configure_owned_target(SIRX11InputBridgeTests)
    endif()
endif()
