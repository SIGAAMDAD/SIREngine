# Core-owned file, mapped-file, and anonymous-pipe services.
foreach(sirTest IN ITEMS SIRPlatformFileTests SIRPlatformFileMappingTests SIRPlatformPipeTests)
    add_executable(${sirTest} ${PROJECT_SOURCE_DIR}/Tests/Unit/${sirTest}.cpp)
    target_link_libraries(${sirTest} PRIVATE SIR::Core)
    add_test(NAME core.platform.${sirTest} COMMAND ${sirTest})
    sir_configure_owned_target(${sirTest})
endforeach()

add_executable(SIRSemaphoreTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRSemaphoreTests.cpp)
target_link_libraries(SIRSemaphoreTests PRIVATE SIR::Core)
add_test(NAME core.platform.semaphore COMMAND SIRSemaphoreTests)
sir_configure_owned_target(SIRSemaphoreTests)

# Platform process, filesystem and current-thread controls.
add_executable(SIRChildProcessTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRChildProcessTests.cpp)
target_link_libraries(SIRChildProcessTests PRIVATE SIR::Core)
add_test(NAME core.platform.child-process COMMAND SIRChildProcessTests)
sir_configure_owned_target(SIRChildProcessTests)

add_executable(SIRPlatformFileSystemTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRPlatformFileSystemTests.cpp)
target_link_libraries(SIRPlatformFileSystemTests PRIVATE SIR::Core)
add_test(NAME core.platform.filesystem COMMAND SIRPlatformFileSystemTests)
sir_configure_owned_target(SIRPlatformFileSystemTests)

add_executable(SIRPlatformThreadControlTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRPlatformThreadControlTests.cpp)
target_link_libraries(SIRPlatformThreadControlTests PRIVATE SIR::Core)
add_test(NAME core.platform.thread-control COMMAND SIRPlatformThreadControlTests)
sir_configure_owned_target(SIRPlatformThreadControlTests)


# Extended cross-platform I/O regression matrix, including descriptor and
# handle lifecycle, concurrent positional writes, mapping, pipe and IPC tests.
add_executable(SIRPlatformIORegressionTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRPlatformIORegressionTests.cpp)
target_link_libraries(SIRPlatformIORegressionTests PRIVATE SIR::Core)
add_test(NAME core.platform.io.regression COMMAND SIRPlatformIORegressionTests)
set_tests_properties(core.platform.io.regression PROPERTIES TIMEOUT 120)
sir_configure_owned_target(SIRPlatformIORegressionTests)

# Production Linux syscalls are intercepted at the final test link boundary.
# Fault injection does not modify the shipping platform implementation.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRPlatformIOFaultInjectionTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRPlatformIOFaultInjectionTests.cpp)
    target_link_libraries(SIRPlatformIOFaultInjectionTests PRIVATE SIR::Core)
    foreach(sirWrappedCall IN ITEMS open pread pwrite write mmap msync fstat stat fsync ftruncate close sysconf munmap pipe2 sem_wait sem_timedwait posix_spawnp)
        target_link_options(SIRPlatformIOFaultInjectionTests PRIVATE "LINKER:--wrap=${sirWrappedCall}")
    endforeach()
    add_test(NAME core.platform.io.fault-injection COMMAND SIRPlatformIOFaultInjectionTests)
    set_tests_properties(core.platform.io.fault-injection PROPERTIES TIMEOUT 60)
    sir_configure_owned_target(SIRPlatformIOFaultInjectionTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRGRDGDeathTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGRDGDeathTests.cpp)
    target_link_libraries(SIRGRDGDeathTests PRIVATE SIR::Resources)
    target_link_options(SIRGRDGDeathTests PRIVATE "LINKER:--wrap=abort")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_FLAGS MATCHES "--coverage")
        target_compile_definitions(SIRGRDGDeathTests PRIVATE SIR_GRDG_GCOV_ENABLED=1)
    endif()
    add_test(NAME resources.lifetime.death COMMAND SIRGRDGDeathTests)
    sir_configure_owned_target(SIRGRDGDeathTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRTaskSchedulerFaultInjectionTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRTaskSchedulerFaultInjectionTests.cpp)
    target_link_libraries(SIRTaskSchedulerFaultInjectionTests PRIVATE SIR::Core)
    target_link_options(SIRTaskSchedulerFaultInjectionTests PRIVATE "LINKER:--wrap=pthread_create")
    add_test(NAME core.jobs.thread-fault-injection COMMAND SIRTaskSchedulerFaultInjectionTests)
    sir_configure_owned_target(SIRTaskSchedulerFaultInjectionTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRPlatformClockProcessFaultTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRPlatformClockProcessFaultTests.cpp)
    target_link_libraries(SIRPlatformClockProcessFaultTests PRIVATE SIR::Core)
    foreach(sirWrappedCall IN ITEMS clock_gettime sysconf nanosleep)
        target_link_options(SIRPlatformClockProcessFaultTests PRIVATE "LINKER:--wrap=${sirWrappedCall}")
    endforeach()
    add_test(NAME core.platform.clock-process-faults COMMAND SIRPlatformClockProcessFaultTests)
    sir_configure_owned_target(SIRPlatformClockProcessFaultTests)
endif()


# The original OpenGL/WGL hidden native HWND lifecycle now belongs to Core/Platform.
if(WIN32)
    add_executable(SIRWGLBootstrapWindowTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWGLBootstrapWindowTests.cpp)
    target_link_libraries(SIRWGLBootstrapWindowTests PRIVATE SIR::Core)
    add_test(NAME core.platform.window.wgl-bootstrap COMMAND SIRWGLBootstrapWindowTests)
    set_tests_properties(core.platform.window.wgl-bootstrap PROPERTIES LABELS "core;platform;window;wgl;integration" TIMEOUT 60)
    sir_configure_owned_target(SIRWGLBootstrapWindowTests)
endif()

# Moved editor host lifecycle test: the shared native host is owned by Core.
add_executable(SIRNativeWindowHostTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRNativeWindowHostTests.cpp)
target_link_libraries(SIRNativeWindowHostTests PRIVATE SIR::Core)
add_test(NAME core.platform.window.native-host COMMAND SIRNativeWindowHostTests)
set_tests_properties(core.platform.window.native-host PROPERTIES LABELS "core;platform;window;integration" TIMEOUT 60)
sir_configure_owned_target(SIRNativeWindowHostTests)

# Window system registry and headless negative-path acceptance.
add_executable(SIRWindowSystemTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWindowSystemTests.cpp)
target_link_libraries(SIRWindowSystemTests PRIVATE SIR::Core)
add_test(NAME core.platform.window.system COMMAND SIRWindowSystemTests)
set_tests_properties(core.platform.window.system PROPERTIES LABELS "core;platform;window" TIMEOUT 60)
sir_configure_owned_target(SIRWindowSystemTests)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SIR_WITH_X11_WINDOW)
    find_package(X11 QUIET)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SIR_WITH_X11_WINDOW AND X11_FOUND)
    find_program(SIR_WINDOW_XVFB_RUN NAMES xvfb-run)
    if(SIR_WINDOW_XVFB_RUN)
        add_executable(SIRWindowX11DriverTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWindowX11DriverTests.cpp)
        target_link_libraries(SIRWindowX11DriverTests PRIVATE SIR::Core X11::X11)
        add_test(NAME core.platform.window.x11-native COMMAND "${SIR_WINDOW_XVFB_RUN}" -a "$<TARGET_FILE:SIRWindowX11DriverTests>")
        set_tests_properties(core.platform.window.x11-native PROPERTIES LABELS "core;platform;window;x11;integration" TIMEOUT 60)
        sir_configure_owned_target(SIRWindowX11DriverTests)
    endif()
endif()

# Native Win32 class ownership and two-driver lifecycle regression.
if(WIN32)
    add_executable(SIRWindowWin32DriverTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWindowWin32DriverTests.cpp)
    target_link_libraries(SIRWindowWin32DriverTests PRIVATE SIR::Core)
    add_test(NAME core.platform.window.win32-native COMMAND SIRWindowWin32DriverTests)
    set_tests_properties(core.platform.window.win32-native PROPERTIES LABELS "core;platform;window;win32;integration" TIMEOUT 60)
    sir_configure_owned_target(SIRWindowWin32DriverTests)
endif()

# Renderer-side pinned native window lease adapter.
add_executable(SIRWindowSurfaceAdapterTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWindowSurfaceAdapterTests.cpp)
target_link_libraries(SIRWindowSurfaceAdapterTests PRIVATE SIR::Graphics)
add_test(NAME core.platform.window.surface-bridge COMMAND SIRWindowSurfaceAdapterTests)
set_tests_properties(core.platform.window.surface-bridge PROPERTIES LABELS "core;platform;window;renderer" TIMEOUT 60)
sir_configure_owned_target(SIRWindowSurfaceAdapterTests)

# One native event pump routes X11 keyboard/text events to the input backend.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SIR_WITH_X11_INPUT AND X11_FOUND)
    find_program(SIR_INPUT_BRIDGE_XVFB NAMES xvfb-run)
    if(SIR_INPUT_BRIDGE_XVFB)
        add_executable(SIRX11InputBridgeTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRX11InputBridgeTests.cpp)
        target_link_libraries(SIRX11InputBridgeTests PRIVATE SIR::Core X11::X11)
        add_test(NAME core.platform.window.x11-input-bridge COMMAND "${SIR_INPUT_BRIDGE_XVFB}" -a "$<TARGET_FILE:SIRX11InputBridgeTests>")
        set_tests_properties(core.platform.window.x11-input-bridge PROPERTIES LABELS "core;platform;input;x11;integration" TIMEOUT 60)
        sir_configure_owned_target(SIRX11InputBridgeTests)
    endif()
endif()

# Shared platform PODs and native-window borrowed view contracts.
add_executable(SIRPlatformContextTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRPlatformContextTests.cpp)
target_link_libraries(SIRPlatformContextTests PRIVATE SIR::Core)
add_test(NAME core.platform.native-context COMMAND SIRPlatformContextTests)
set_tests_properties(core.platform.native-context PROPERTIES LABELS "core;platform;input;audio;renderer;abi" TIMEOUT 60)
sir_configure_owned_target(SIRPlatformContextTests)

# Compile the actual Wayland driver against a deterministic fake compositor.
# This test does not require the optional Wayland development packages.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRWaylandWindowDriverProtocolTests
        ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWaylandWindowDriverProtocolTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Window/Linux/SIRWindowDriverWayland.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/SIRWaylandInputBridge.cpp")
    target_include_directories(SIRWaylandWindowDriverProtocolTests BEFORE PRIVATE
        "${PROJECT_SOURCE_DIR}/Tests/Unit/Fixtures/Wayland")
    target_link_libraries(SIRWaylandWindowDriverProtocolTests PRIVATE SIR::Core)
    target_compile_definitions(SIRWaylandWindowDriverProtocolTests PRIVATE
        SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE=1 SIR_WAYLAND_XKB_AVAILABLE=1
        SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE=1)
    add_test(NAME core.platform.window.wayland-protocol COMMAND SIRWaylandWindowDriverProtocolTests)
    set_tests_properties(core.platform.window.wayland-protocol PROPERTIES LABELS "core;platform;window;wayland;protocol" TIMEOUT 60)
    sir_configure_owned_target(SIRWaylandWindowDriverProtocolTests)
endif()

# Application window-pump ordering and tri-state status regression retained from develop.
add_executable(SIRApplicationWindowTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRApplicationWindowTests.cpp)
target_link_libraries(SIRApplicationWindowTests PRIVATE SIR::Core)
add_test(NAME core.application.unconditional-window-pump COMMAND SIRApplicationWindowTests)
sir_configure_owned_target(SIRApplicationWindowTests)
