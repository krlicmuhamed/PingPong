# CMakeList.txt : CMake project for InlineAssembly, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


# Add Windows Implementation Libraries
#find_package(WIL CONFIG REQUIRED)

# Add imgui library
#find_package(imgui CONFIG REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Set default compile flags for GCC
if(CMAKE_COMPILER_IS_GNUCXX)
    message(STATUS "GCC detected, adding compile flags")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -pedantic -Wall -Wextra") 
    set(CMAKE_CXX_FLAGS "-Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-g")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif(CMAKE_COMPILER_IS_GNUCXX)

string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "/Ob2" "/Ob3" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

string(CONCAT CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" " ")
message(${CMAKE_CXX_FLAGS_RELEASE})

set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/icon.rc")

# Add source to this project's executable.
add_executable (PingPong2 WIN32 "PingPong.cpp" "PingPong.h" ${APP_ICON_RESOURCE_WINDOWS})

target_link_libraries(PingPong2 PRIVATE
    gdiplus.lib
    d2d1.lib
	d3d12.lib
	dxgi.lib
)
