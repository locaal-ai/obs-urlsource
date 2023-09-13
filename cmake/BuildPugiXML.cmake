include(ExternalProject)

if(APPLE)
  set(PUGIXML_CMAKE_PLATFORM_OPTIONS -DCMAKE_OSX_ARCHITECTURES=x86_64$<SEMICOLON>arm64)
else()
  set(PUGIXML_CMAKE_PLATFORM_OPTIONS "")
endif()

ExternalProject_Add(
  pugixml_build
  URL https://github.com/zeux/pugixml/releases/download/v1.13/pugixml-1.13.tar.gz
  URL_MD5 3e4c588e03bdca140844f3c47c1a995e
  INSTALL_BYPRODUCTS <INSTALL_DIR>/lib/${CMAKE_STATIC_LIBRARY_PREFIX}pugixml${CMAKE_STATIC_LIBRARY_SUFFIX}
  CMAKE_GENERATOR ${CMAKE_GENERATOR}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DBUILD_SHARED_LIBS=OFF -DPUGIXML_BUILD_TESTS=OFF
             ${PUGIXML_CMAKE_PLATFORM_OPTIONS})

ExternalProject_Get_Property(pugixml_build INSTALL_DIR)

message(STATUS "pugixml will be installed to ${INSTALL_DIR}")

# find the library
set(pugixml_lib_filename ${CMAKE_STATIC_LIBRARY_PREFIX}pugixml${CMAKE_STATIC_LIBRARY_SUFFIX})
set(pugixml_lib_location ${INSTALL_DIR}/lib/${pugixml_lib_filename})

message(STATUS "pugixml library expected at ${pugixml_lib_location}")

add_library(pugixml STATIC IMPORTED)
set_target_properties(pugixml PROPERTIES IMPORTED_LOCATION ${pugixml_lib_location})

add_library(libpugixml INTERFACE)
add_dependencies(libpugixml pugixml_build)
target_link_libraries(libpugixml INTERFACE pugixml)
set_target_properties(libpugixml PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)
