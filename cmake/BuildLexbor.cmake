include(ExternalProject)

if(APPLE)
  set(LEXBOR_CMAKE_PLATFORM_OPTIONS -DCMAKE_OSX_ARCHITECTURES=x86_64$<SEMICOLON>arm64)
else()
  set(LEXBOR_CMAKE_PLATFORM_OPTIONS "")
endif()

ExternalProject_Add(
  lexbor_build
  GIT_REPOSITORY https://github.com/lexbor/lexbor.git
  GIT_TAG v2.3.0
  CMAKE_GENERATOR ${CMAKE_GENERATOR}
  INSTALL_BYPRODUCTS <INSTALL_DIR>/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lexbor${CMAKE_STATIC_LIBRARY_SUFFIX}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DLEXBOR_BUILD_SHARED=OFF -DLEXBOR_BUILD_STATIC=ON
             ${LEXBOR_CMAKE_PLATFORM_OPTIONS})

ExternalProject_Get_Property(lexbor_build INSTALL_DIR)

message(STATUS "lexbor will be installed to ${INSTALL_DIR}")

# find the library
set(lexbor_lib_filename ${CMAKE_STATIC_LIBRARY_PREFIX}lexbor_static${CMAKE_STATIC_LIBRARY_SUFFIX})
set(lexbor_lib_location ${INSTALL_DIR}/lib/${lexbor_lib_filename})

message(STATUS "lexbor library expected at ${lexbor_lib_location}")

add_library(lexbor_internal STATIC IMPORTED)
set_target_properties(lexbor_internal PROPERTIES IMPORTED_LOCATION ${lexbor_lib_location})

add_library(liblexbor_internal INTERFACE)
add_dependencies(liblexbor_internal lexbor_build)
target_link_libraries(liblexbor_internal INTERFACE lexbor_internal)
set_target_properties(liblexbor_internal PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)
