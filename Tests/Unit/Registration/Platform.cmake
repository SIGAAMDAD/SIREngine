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


# One native window owner, generation-checked renderer borrows, and event-pump lifecycle.
add_executable(SIRWindowSystemTests SIRWindowSystemTests.cpp)
target_link_libraries(SIRWindowSystemTests PRIVATE SIR::Core SIR::Graphics)
add_test(NAME core.window.registry-leases COMMAND SIRWindowSystemTests)
sir_configure_owned_target(SIRWindowSystemTests)

add_executable(SIRApplicationWindowTests SIRApplicationWindowTests.cpp)
target_link_libraries(SIRApplicationWindowTests PRIVATE SIR::Core)
add_test(NAME core.application.unconditional-window-pump COMMAND SIRApplicationWindowTests)
sir_configure_owned_target(SIRApplicationWindowTests)

add_test(NAME launcher.headless-frames COMMAND SIREngine --headless --frames 5)
add_test(NAME launcher.headless-bad-frames COMMAND SIREngine --headless --frames nope)
set_tests_properties(launcher.headless-bad-frames PROPERTIES WILL_FAIL TRUE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 QUIET)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND X11_FOUND)
    add_executable(SIRWindowX11SystemTests SIRWindowX11SystemTests.cpp)
    target_link_libraries(SIRWindowX11SystemTests PRIVATE SIR::Core SIR::Graphics X11::X11)
    find_program(SIR_XVFB_RUN_EXECUTABLE NAMES xvfb-run)
    if(SIR_XVFB_RUN_EXECUTABLE)
        add_test(NAME core.window.x11-native COMMAND "${SIR_XVFB_RUN_EXECUTABLE}" -a "$<TARGET_FILE:SIRWindowX11SystemTests>")
        add_test(NAME launcher.x11-native-frames COMMAND "${SIR_XVFB_RUN_EXECUTABLE}" -a "$<TARGET_FILE:SIREngine>" --frames 5)
        set_tests_properties(core.window.x11-native launcher.x11-native-frames PROPERTIES TIMEOUT 30 RESOURCE_LOCK sir_xvfb)
    endif()
    sir_configure_owned_target(SIRWindowX11SystemTests)
endif()

add_executable(SIRWindowServiceTests SIRWindowServiceTests.cpp)
target_link_libraries(SIRWindowServiceTests PRIVATE SIR::Core)
add_test(NAME core.window.application-service-order COMMAND SIRWindowServiceTests)
sir_configure_owned_target(SIRWindowServiceTests)
