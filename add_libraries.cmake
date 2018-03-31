message("Build Generator: " ${CMAKE_GENERATOR})
message("Build Platform: " ${CMAKE_GENERATOR_PLATFORM})
set(ASSIMP_CXX_FLAGS 
    /bigobj 
    /DASSIMP_BUILD_NO_EXPORT 
    /DASSIMP_BUILD_NO_AMF_IMPORTER 
    /DASSIMP_BUILD_NO_AC_IMPORTER 
    /DASSIMP_BUILD_NO_ASE_IMPORTER 
    /DASSIMP_BUILD_NO_ASSBIN_IMPORTER 
    /DASSIMP_BUILD_NO_ASSXML_IMPORTER 
    /DASSIMP_BUILD_NO_B3D_IMPORTER 
    /DASSIMP_BUILD_NO_BVH_IMPORTER 
    /DASSIMP_BUILD_NO_DXF_IMPORTER 
    /DASSIMP_BUILD_NO_CSM_IMPORTER 
    /DASSIMP_BUILD_NO_HMP_IMPORTER 
    /DASSIMP_BUILD_NO_IRRMESH_IMPORTER 
    /DASSIMP_BUILD_NO_IRR_IMPORTER 
    /DASSIMP_BUILD_NO_LWO_IMPORTER 
    /DASSIMP_BUILD_NO_LWS_IMPORTER 
    /DASSIMP_BUILD_NO_NFF_IMPORTER 
    /DASSIMP_BUILD_NO_NDO_IMPORTER 
    /DASSIMP_BUILD_NO_OFF_IMPORTER 
    /DASSIMP_BUILD_NO_MS3D_IMPORTER 
    /DASSIMP_BUILD_NO_COB_IMPORTER 
    /DASSIMP_BUILD_NO_IFC_IMPORTER 
    /DASSIMP_BUILD_NO_XGL_IMPORTER 
    /DASSIMP_BUILD_NO_SIB_IMPORTER 
    /DASSIMP_BUILD_NO_3D_IMPORTER 
    /DASSIMP_BUILD_NO_X_IMPORTER 
    /DASSIMP_BUILD_NO_X3D_IMPORTER 
    /DASSIMP_BUILD_NO_GLTF_IMPORTER 
    /DASSIMP_BUILD_NO_3MF_IMPORTER 
    /DASSIMP_BUILD_NO_MMD_IMPORTER)
set(EXT_LIB_ARGS 
    -DCMAKE_CXX_FLAGS=${ASSIMP_CXX_FLAGS}
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF 
    -DASSIMP_BUILD_TESTS=OFF
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_SOURCE_DIR}/external/cached/assimp
    -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
    -DCMAKE_GENERATOR=${CMAKE_GENERATOR})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/assimp/build)
execute_process(COMMAND ${CMAKE_COMMAND} ${EXT_LIB_DEFAULTS} ${EXT_LIB_ARGS} ${PROJECT_SOURCE_DIR}/submodules/assimp
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/assimp/build)
execute_process(COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/submodules/assimp/build --config Release --target install
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/assimp/build)

set(EXT_LIB_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_SOURCE_DIR}/external/cached/glm -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_GENERATOR=${CMAKE_GENERATOR} -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/glm/build)
execute_process(COMMAND ${CMAKE_COMMAND} ${EXT_LIB_DEFAULTS} ${EXT_LIB_ARGS} ${PROJECT_SOURCE_DIR}/submodules/glm
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/glm/build)
execute_process(COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/submodules/glm/build --target install
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/glm/build)

set(EXT_LIB_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_SOURCE_DIR}/external/cached/pugixml -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_GENERATOR=${CMAKE_GENERATOR} -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/pugixml/build)
execute_process(COMMAND ${CMAKE_COMMAND} ${EXT_LIB_DEFAULTS} ${EXT_LIB_ARGS} ${PROJECT_SOURCE_DIR}/submodules/pugixml
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/pugixml/build)
execute_process(COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/submodules/pugixml/build --target install
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/pugixml/build)

set(EXT_LIB_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_SOURCE_DIR}/external/cached/mygl -DCMAKE_BUILD_TYPE=Release -DCMAKE_GENERATOR=${CMAKE_GENERATOR} -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM} -DMYGL_SETTINGS_PATH="\"${PROJECT_SOURCE_DIR}/src/res/mygl_settings.xml\"")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/mygl/build)
execute_process(COMMAND ${CMAKE_COMMAND} ${EXT_LIB_DEFAULTS} ${EXT_LIB_ARGS} ${PROJECT_SOURCE_DIR}/submodules/mygl
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/mygl/build)
execute_process(COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/submodules/mygl/build --target install
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/mygl/build)
    
set(EXT_LIB_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_SOURCE_DIR}/external/cached/openal-soft
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    -DALSOFT_UTILS=OFF 
    -DALSOFT_EXAMPLES=OFF 
    -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
    -DALSOFT_TESTS=OFF
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_GENERATOR=${CMAKE_GENERATOR})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/openal-soft/build)
execute_process(COMMAND ${CMAKE_COMMAND} ${EXT_LIB_DEFAULTS} ${EXT_LIB_ARGS} ${PROJECT_SOURCE_DIR}/submodules/openal-soft
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/openal-soft/build)
execute_process(COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/submodules/openal-soft/build --target install
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/openal-soft/build)

set(EXT_LIB_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_SOURCE_DIR}/external/cached/glfw3
    -DGLFW_BUILD_TESTS=OFF 
    -DGLFW_BUILD_DOCS=OFF 
    -DGLFW_BUILD_EXAMPLES=OFF
    -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_GENERATOR=${CMAKE_GENERATOR})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/glfw3/build)
execute_process(COMMAND ${CMAKE_COMMAND} ${EXT_LIB_DEFAULTS} ${EXT_LIB_ARGS} ${PROJECT_SOURCE_DIR}/submodules/glfw3
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/glfw3/build)
execute_process(COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/submodules/glfw3/build --target install
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/glfw3/build)
    
set(EXT_LIB_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_SOURCE_DIR}/external/cached/glshader -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_GENERATOR=${CMAKE_GENERATOR} -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/glshader/build)
execute_process(COMMAND ${CMAKE_COMMAND} ${EXT_LIB_DEFAULTS} ${EXT_LIB_ARGS} ${PROJECT_SOURCE_DIR}/submodules/glshader
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/glshader/build)
execute_process(COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/submodules/glshader/build --target install
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/submodules/glshader/build)