include_directories(${PROJECT_SOURCE_DIR}/src/include)

aux_source_directory(${PROJECT_SOURCE_DIR}/src proto)

add_library(${PROJECT_NAME} ${proto})

if(${OPT})
  message(STATUS "enable optimization, level=${OPT}")
  add_compile_options(${PROJECT_NAME} PRIVATE -O${OPT})
else()
  message(STATUS "disable optimization")
endif()