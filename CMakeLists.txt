# CMakeLists.txt: CMake project for the Kitsune Engine
# Root directory CMakeList

cmake_minimum_required(VERSION 4.0)
message(STATUS "Building using CMAKE Version: ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}")

project(AngelBase VERSION 0.1 LANGUAGES C CXX)

#needs Vulkan
find_package(Vulkan REQUIRED)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#organizing filters by directory
set_property(GLOBAL PROPERTY USE_FOLDERS ON)



file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp"
)


#creating engine executable
add_executable(AngelBase ${SOURCES})

add_subdirectory(lib)
add_subdirectory(assets)

#Add all files into the correct folder
file(GLOB_RECURSE _source_list *.cpp* *.h* *.hpp*)
foreach(_source IN ITEMS ${_source_list})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_group_path}")
    source_group("${_group_path}" FILES "${_source}")
    message(STATUS "Adding Source: ${_source} to subfilter ${_group_path}")
endforeach()



#Shader Compilation target
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
