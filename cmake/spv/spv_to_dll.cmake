set(SPVGEN_CURRENT_DIR ${CMAKE_CURRENT_LIST_DIR})  

function(add_shader SPVGEN_LIB_NAME)
	list(REMOVE_AT ARGV 0)

	add_library(${SPVGEN_LIB_NAME} SHARED)
	find_program(GLSLC_COMMAND glslc)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/__tmp_shader_${SPVGEN_LIB_NAME}.cpp "#pragma once\n")
	file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/__tmp_shader_${SPVGEN_LIB_NAME}.cpp "// This file is auto-generated by cmake\n\n")
	foreach(SPVGEN_CURRENT_FILE ${ARGV})
		unset(FILE_DEPENDENCIES)
		execute_process(COMMAND ${GLSLC_COMMAND} -M "${SPVGEN_CURRENT_FILE}" OUTPUT_VARIABLE FILE_DEPENDENCIES ERROR_VARIABLE FILE_ERROR)
		if(FILE_ERROR)
			message(FATAL_ERROR ${FILE_ERROR})
		endif()

		set(SPVGEN_CURRENT_FILE_FULL_PATH ${SPVGEN_CURRENT_FILE})
		file(RELATIVE_PATH SPVGEN_CURRENT_FILE ${CMAKE_CURRENT_LIST_DIR}/ ${SPVGEN_CURRENT_FILE})
		set(SPVGEN_GENERATED_SPV "@SPVGEN_GENERATED_SPV@")

		string(REGEX REPLACE "[- .\\/]" "_" SPVGEN_SHADER_NAME ${SPVGEN_CURRENT_FILE})
		configure_file(${SPVGEN_CURRENT_DIR}/shader_template.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${SPVGEN_CURRENT_FILE}.hpp.in @ONLY)

		file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/__tmp_shader_${SPVGEN_LIB_NAME}.cpp "#include \"${SPVGEN_CURRENT_FILE}.hpp\"\n")
		string(REPLACE "\n" "" FILE_DEPENDENCIES ${FILE_DEPENDENCIES})
		string(REPLACE " " ";" FILE_DEPENDENCY_LIST ${FILE_DEPENDENCIES})
		list(REMOVE_AT FILE_DEPENDENCY_LIST 0)
		
		ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${SPVGEN_CURRENT_FILE}.hpp
			COMMAND ${CMAKE_COMMAND} 
				-DSPVGEN_GLSLC_COMMAND=${GLSLC_COMMAND}
				-DSPVGEN_INCLUDES="$<JOIN:$<TARGET_PROPERTY:${SPVGEN_LIB_NAME},INCLUDE_DIRECTORIES>, -I>"
				-DSPVGEN_DEFINITIONS="$<JOIN:$<TARGET_PROPERTY:${SPVGEN_LIB_NAME},COMPILE_DEFINITIONS>, -D>"
				-DSPVGEN_ARGS="$<JOIN:$<TARGET_PROPERTY:${SPVGEN_LIB_NAME},COMPILE_OPTIONS>, >$<JOIN:$<TARGET_PROPERTY:${SPVGEN_LIB_NAME},COMPILE_FLAGS>, >"
				-DSPVGEN_INPUT=${SPVGEN_CURRENT_FILE_FULL_PATH}
				-DSPVGEN_HEADER=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${SPVGEN_CURRENT_FILE}.hpp
				-P ${SPVGEN_CURRENT_DIR}/compile.cmake
			DEPENDS ${FILE_DEPENDENCY_LIST} ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${SPVGEN_CURRENT_FILE}.hpp.in
		)
		list(APPEND SPVGEN_DEPENDENT_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${SPVGEN_CURRENT_FILE}.hpp)
	endforeach()

	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/__shader_${SPVGEN_LIB_NAME}.cpp
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/__tmp_shader_${SPVGEN_LIB_NAME}.cpp ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/__shader_${SPVGEN_LIB_NAME}.cpp
		DEPENDS ${SPVGEN_DEPENDENT_HEADERS})

	target_sources(${SPVGEN_LIB_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/__shader_${SPVGEN_LIB_NAME}.cpp)
	target_compile_features(${SPVGEN_LIB_NAME} PUBLIC cxx_std_17)
endfunction()