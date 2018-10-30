include(CMakeLanguageInformation)
set(CMAKE_SPV_OUTPUT_EXTENSION .spv)
set(CMAKE_SPV_IGNORE_EXTENSIONS glsl;h)
set(CMAKE_SPV_SOURCE_FILE_EXTENSIONS vert;frag;tesc;tese;comp;geom)

set(CMAKE_SPV_COMPILE_OBJECT "<CMAKE_SPV_COMPILER> <FLAGS> <DEFINES> <INCLUDES> <SOURCE> -o <OBJECT>")
set(CMAKE_SPV_CREATE_SHARED_LIBRARY "<CMAKE_COMMAND> -E copy <OBJECTS> <TARGET_BASE>")
set(CMAKE_SPV_ARCHIVE_CREATE "")
set(CMAKE_SPV_ARCHIVE_FINISH "")
set(CMAKE_SPV_COMPILER_ENV_VAR "SPV")
set(CMAKE_SPV_INFORMATION_LOADED 1)