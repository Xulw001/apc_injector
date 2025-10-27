set(project_name injCli)
project ("${project_name}")

include_directories(${INJ_ROOT}/include)
aux_source_directory(. SOURCES)
add_executable(${project_name} ${SOURCES})