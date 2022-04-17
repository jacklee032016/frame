# add THIS_FILE 
# call with `define_relative_file_paths ${SOURCES}`

function (define_relative_file_paths SOURCES)
  foreach (SOURCE IN LISTS SOURCES)
    #file (
    #  RELATIVE_PATH RELATIVE_SOURCE_PATH
    #  ${PROJECT_SOURCE_DIR} ${SOURCE}
    #)
	
	# message("File: ${SOURCE}...")
    set_source_files_properties (
      ${SOURCE} PROPERTIES
      COMPILE_DEFINITIONS THIS_FILE="${SOURCE}"
    )
  endforeach ()
endfunction ()
