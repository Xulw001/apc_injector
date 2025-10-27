set(project_name injDemo)
project ("${project_name}")

aux_source_directory(. SOURCES)
add_library(${project_name} SHARED ${SOURCES})
set_target_properties(${project_name} PROPERTIES OUTPUT_NAME "Demo_${ARCH}")