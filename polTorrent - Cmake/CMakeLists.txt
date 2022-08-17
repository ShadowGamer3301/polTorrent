cmake_minimum_required(VERSION 3.10)

# set the project name
project(polTorrent)

# add the executable
add_executable(polTorrent "src/main.cpp")

# add include directories
target_include_directories(polTorrent PUBLIC "/usr/include/")

#link libtorrent library
target_link_libraries(polTorrent "/usr/lib64/libtorrent-rasterbar.so")