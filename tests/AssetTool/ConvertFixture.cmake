# The exact CTest-generated output is disposable; never remove its directory.
get_filename_component(name "${OUTPUT}" NAME)
if(NOT name MATCHES "^e2x-market-stall-(Release|Debug|RelWithDebInfo|)\\.glb$")
    message(FATAL_ERROR "Unexpected render fixture output filename")
endif()
file(REMOVE "${OUTPUT}")
execute_process(COMMAND "${TOOL}" convert "${INPUT}" "${OUTPUT}"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 20)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Fresh render fixture conversion failed: ${result}\n${stdout}\n${stderr}")
endif()
message(STATUS "${stdout}")
