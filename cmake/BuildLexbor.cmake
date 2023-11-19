include(ExternalProject)

if(APPLE)
  set(LEXBOR_CMAKE_PLATFORM_OPTIONS -DCMAKE_OSX_ARCHITECTURES=x86_64$<SEMICOLON>arm64)
else()
  if(WIN32)
    add_compile_definitions(LEXBOR_STATIC=1)
    set(LEXBOR_CMAKE_PLATFORM_OPTIONS "-DCMAKE_C_FLAGS=/W3 /utf-8 /MP" "-DCMAKE_CXX_FLAGS=/W3 /utf-8 /MP")
  else()
    set(LEXBOR_CMAKE_PLATFORM_OPTIONS -DCMAKE_SYSTEM_NAME=Linux)
  endif()
endif()

set(lexbor_lib_filename ${CMAKE_STATIC_LIBRARY_PREFIX}lexbor_static${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add(
  lexbor_build
  GIT_REPOSITORY https://github.com/lexbor/lexbor.git
  GIT_TAG v2.3.0
  CMAKE_GENERATOR ${CMAKE_GENERATOR}
  BUILD_BYPRODUCTS <INSTALL_DIR>/lib/${lexbor_lib_filename} INSTALL_BYPRODUCTS <INSTALL_DIR>/include
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
             -DLEXBOR_BUILD_SHARED=OFF
             -DLEXBOR_BUILD_STATIC=ON
             -DLEXBOR_BUILD_TESTS_CPP=OFF
             -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
             -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
             -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
             -DCMAKE_LINKER=${CMAKE_LINKER}
             -DCMAKE_INSTALL_MESSAGE=NEVER
             ${LEXBOR_CMAKE_PLATFORM_OPTIONS})

ExternalProject_Get_Property(lexbor_build INSTALL_DIR)

set(lexbor_lib_location ${INSTALL_DIR}/lib/${lexbor_lib_filename})

add_library(lexbor_internal STATIC IMPORTED)
add_dependencies(lexbor_internal lexbor_build)
set_target_properties(lexbor_internal PROPERTIES IMPORTED_LOCATION ${lexbor_lib_location})
target_include_directories(lexbor_internal INTERFACE ${INSTALL_DIR}/include)

add_library(liblexbor_internal INTERFACE)
add_dependencies(liblexbor_internal lexbor_internal lexbor_build)
target_link_libraries(liblexbor_internal INTERFACE lexbor_internal)
