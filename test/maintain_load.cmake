# Test argument validation before running any continuous workload.
set(invalid_options
    "--cpu-target=30"
    "--maintain-load --cpu-target=0"
    "--maintain-load --cpu-target=101"
    "--maintain-load --cpu-target=invalid"
    "--maintain-load --threads=2"
    "--maintain-load --threads=0"
    "--maintain-load --count"
    "--maintain-load --count=0"
    "--maintain-load --dist=100"
    "--maintain-load=invalid"
    "--maintain-load --print"
    "--maintain-load 100"
    "--maintain-load --stress-test"
    "--maintain-load --timeout=invalid")

foreach(options ${invalid_options})
    separate_arguments(args UNIX_COMMAND "${options}")
    execute_process(COMMAND "${PRIMESIEVE}" ${args}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output
                    ERROR_VARIABLE error TIMEOUT 2)
    if(NOT "${result}" STREQUAL "1" OR NOT error MATCHES "primesieve:")
        message(FATAL_ERROR "Invalid options not rejected: ${options}: ${result} ${output} ${error}")
    endif()
endforeach()

execute_process(COMMAND "${PRIMESIEVE}" --maintain-load --cpu-target=30 --threads=1 --timeout=2s
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 5)
if(LINUX)
    if(NOT "${result}" STREQUAL "0" OR NOT output MATCHES "Stopped after")
        message(FATAL_ERROR "Finite load run failed: ${result} ${output} ${error}")
    endif()
    execute_process(COMMAND "${PRIMESIEVE}" --maintain-load --quiet --timeout=1s
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 4)
    if(NOT "${result}" STREQUAL "0" OR NOT "${output}" STREQUAL "" OR NOT "${error}" STREQUAL "")
        message(FATAL_ERROR "Quiet load run failed: ${result} ${output} ${error}")
    endif()
elseif(NOT "${result}" STREQUAL "1" OR NOT error MATCHES "requires Linux")
    message(FATAL_ERROR "Expected unsupported-platform error: ${result} ${output} ${error}")
endif()
