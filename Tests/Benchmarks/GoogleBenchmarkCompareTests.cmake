cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED SIR_ROOT OR NOT DEFINED SIR_TEST_ROOT)
    message(FATAL_ERROR "Missing benchmark comparison test root")
endif()

set(sirCurrent "${SIR_TEST_ROOT}/current")
set(sirBaseline "${SIR_TEST_ROOT}/baseline")
set(sirReport "${SIR_TEST_ROOT}/report.md")
file(REMOVE_RECURSE "${SIR_TEST_ROOT}")
file(MAKE_DIRECTORY "${sirCurrent}" "${sirBaseline}/benchmarks")

set(sirJson [=[
{
  "benchmarks": [
    {
      "name": "SIRBenchmarkApplicationTick_median",
      "cpu_time": __CPU__,
      "time_unit": "ns",
      "aggregate_name": "median"
    }
  ]
}
]=])
string(REPLACE "__CPU__" "110.0" sirCurrentJson "${sirJson}")
string(REPLACE "__CPU__" "100.0" sirBaselineJson "${sirJson}")
file(WRITE "${sirCurrent}/ubuntu-24.04.json" "${sirCurrentJson}")
file(WRITE "${sirBaseline}/benchmarks/ubuntu-24.04.json" "${sirBaselineJson}")

execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DSIR_CURRENT_DIR=${sirCurrent}"
        "-DSIR_BASELINE_DIR=${sirBaseline}"
        "-DSIR_REPORT=${sirReport}"
        -P "${SIR_ROOT}/Build/CMake/GoogleBenchmarkCompare.cmake"
    RESULT_VARIABLE sirStatus)
if(NOT sirStatus EQUAL 0)
    message(FATAL_ERROR "Google Benchmark comparison script failed")
endif()
file(READ "${sirReport}" sirText)
if(NOT sirText MATCHES "ubuntu-24\\.04")
    message(FATAL_ERROR "The reporter damaged a dotted runner label")
endif()
if(NOT sirText MATCHES "Matched measurements against previous successful nightly: 1")
    message(FATAL_ERROR "The reporter failed to compare the existing baseline")
endif()
if(NOT sirText MATCHES "110\\.0 \\| 100\\.0")
    message(FATAL_ERROR "The reporter failed to retain current and baseline CPU timings")
endif()
