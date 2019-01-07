get_filename_component(SPVGEN_CURRENT_BINARY_DIR ${SPVGEN_HEADER} DIRECTORY)
message("[SPIRV] Compiling ${SPVGEN_INPUT} ...")
if(NOT EXISTS ${SPVGEN_CURRENT_BINARY_DIR})
	file(MAKE_DIRECTORY ${SPVGEN_CURRENT_BINARY_DIR})
endif()
get_filename_component(SPVGEN_INPUT_NAME ${SPVGEN_INPUT} NAME)
execute_process(COMMAND ${SPVGEN_GLSLC_COMMAND} -mfmt=c "${SPVGEN_INPUT}" -o "${SPVGEN_CURRENT_BINARY_DIR}/gen-${SPVGEN_INPUT_NAME}")
file(READ ${SPVGEN_CURRENT_BINARY_DIR}/gen-${SPVGEN_INPUT_NAME} SPVGEN_GENERATED_SPV)
configure_file(${SPVGEN_HEADER}.in ${SPVGEN_HEADER} @ONLY)
file(REMOVE ${SPVGEN_CURRENT_BINARY_DIR}/gen-${SPVGEN_INPUT_NAME})