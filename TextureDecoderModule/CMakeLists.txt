# Define target name and output directory
init_target (TextureDecoderModule OUTPUT modules/core)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB XML_FILES *.xml)
set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

set (FILES_TO_TRANSLATE ${FILES_TO_TRANSLATE} ${H_FILES} ${CPP_FILES} PARENT_SCOPE)

add_definitions (-DTEXTURE_DECODER_MODULE_EXPORTS)

use_package (BOOST)
use_package (POCO)
use_package (OPENJPEG)
use_modules (Core Foundation Interfaces RexCommon AssetModule)

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES})

link_modules (Core Foundation Interfaces  RexCommon)
link_package (BOOST)
link_package (POCO)
link_package (OPENJPEG)

# MSVC -specific settings for preprocessor and PCH use
if (MSVC)
    # Label StableHeaders.cpp to create the PCH file and mark all other .cpp files to use that PCH file.
    # Add a #define DEBUG_CPP_NAME "this compilation unit name" to each compilation unit to aid in memory leak checking.
    foreach(src_file ${CPP_FILES})
            if (${src_file} MATCHES "StableHeaders.cpp$")
                set_source_files_properties(${src_file} PROPERTIES COMPILE_FLAGS "/YcStableHeaders.h")        
            else()
                get_filename_component(basename ${src_file} NAME)
                set_source_files_properties(${src_file} PROPERTIES COMPILE_FLAGS "/YuStableHeaders.h -DDEBUG_CPP_NAME=\"\\\"${basename}\"\\\"")
            endif()
    endforeach(src_file ${CPP_FILES})
endif (MSVC)

# Copy XML files from source directory to target directory
foreach (XML_FILE ${XML_FILES})
	add_custom_command (TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${XML_FILE} ${TARGET_DIR})
endforeach (XML_FILE)

final_target ()