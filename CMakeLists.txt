# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("PingPong")

# Include sub-projects.
add_subdirectory ("PingPongGDI")
add_subdirectory ("PingPongGDI_Rewrite")
add_subdirectory ("PingPongD2D")
