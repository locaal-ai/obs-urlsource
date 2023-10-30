
add_library(libtidy INTERFACE)
target_include_directories(libtidy INTERFACE /usr/local/include)
target_link_libraries(libtidy INTERFACE /usr/local/lib/libtidy.a)
