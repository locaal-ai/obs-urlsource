if(WIN32 OR APPLE)
  # Windows and macOS are supported by the prebuilt dependencies

  if(NOT buildspec)
    file(READ "${CMAKE_SOURCE_DIR}/buildspec.json" buildspec)
  endif()

  string(
    JSON
    version
    GET
    ${buildspec}
    dependencies
    prebuilt
    version)

  if(MSVC)
    set(arch ${CMAKE_GENERATOR_PLATFORM})
  elseif(APPLE)
    set(arch universal)
  endif()

  set(deps_root "${CMAKE_SOURCE_DIR}/.deps/obs-deps-${version}-${arch}")
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${deps_root}/include")
else()
  # Linux requires fetching the dependencies
  include(FetchContent)

  FetchContent_Declare(
    websocketpp
    URL https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.tar.gz
    URL_HASH SHA256=6ce889d85ecdc2d8fa07408d6787e7352510750daa66b5ad44aacb47bea76755)

  # Only download the content, don't configure or build it
  FetchContent_GetProperties(websocketpp)

  if(NOT websocketpp_POPULATED)
    FetchContent_Populate(websocketpp)
  endif()

  # Add WebSocket++ as an interface library
  add_library(websocketpp INTERFACE)
  target_include_directories(websocketpp INTERFACE ${websocketpp_SOURCE_DIR})

  # Fetch ASIO
  FetchContent_Declare(
    asio
    URL https://github.com/chriskohlhoff/asio/archive/asio-1-28-0.tar.gz
    URL_HASH SHA256=226438b0798099ad2a202563a83571ce06dd13b570d8fded4840dbc1f97fa328)

  FetchContent_MakeAvailable(websocketpp asio)

  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE websocketpp)
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${asio_SOURCE_DIR}/asio/include/)
endif()

message(STATUS "WebSocket++ and ASIO have been added to the project")
