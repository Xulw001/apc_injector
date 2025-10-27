set(project_name injdrv)
project ("${project_name}")

include_directories(${INJ_ROOT}/include ${RTL})
aux_source_directory(. SOURCES)
wdk_add_driver(${project_name} ${SOURCES})
target_link_libraries(${project_name} PRIVATE libutil)