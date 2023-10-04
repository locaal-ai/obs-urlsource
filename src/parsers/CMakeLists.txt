target_sources(${CMAKE_PROJECT_NAME} PRIVATE jsonpointer.cpp jsonpath.cpp regex.cpp xml.cpp errors.cpp)

# on linux, disable conversion errors
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wno-error -Wno-conversion -Wno-shadow -Wno-unused-parameter)
endif()
