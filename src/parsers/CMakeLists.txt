target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE binary-data.cpp
          errors.cpp
          html.cpp
          jsonpath.cpp
          jsonpointer.cpp
          key-value.cpp
          regex.cpp
          xml.cpp)

# on linux, disable conversion errors
if(UNIX AND NOT APPLE)
  set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
  add_compile_options(-Wno-error -Wno-conversion -Wno-shadow -Wno-unused-parameter)
endif()
