# SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

# Most of this file is taken from pbrt's CMakeLists.txt, except for the FetchContent stuff.

include( CheckCXXSourceCompiles )
include( CheckIncludeFiles )
include( Fetchglog )
include(FetchContent)

string(TIMESTAMP t1 "%s")
message(VERBOSE "Finding pbrt...")

# Allow GIT_SUBMODULES as empty string to avoid cloning submodules.
if( POLICY CMP0097 )
    cmake_policy(SET CMP0097 NEW)
endif()

# Don't warn about FetchContent_Populate
if( POLICY CMP0169 )
    cmake_policy(SET CMP0169 OLD)
endif()

# The pbrt repository does not have any tags, so to avoid the expense of cloning
# the entire repository we use a shallow clone of a fork with a recent tag.
FetchContent_Declare(
  pbrt
  GIT_REPOSITORY https://github.com/MarkLeone/pbrt-v3.git # forked from mmp/pbrt-v3
  GIT_TAG v2023-09-03 # 13d871faae88233b327d04cda24022b8bb0093ee
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
  GIT_SUBMODULES ""
)
# We use Populate instead of MakeAvailable because we don't want add_subdirectory
# called even though pbrt includes a CMakeLists.txt.  We're going to take a slice
# of the pbrt source code and make a parsing library from it and the existing
# CMakeLists.txt in pbrt doesn't organize the code this way, so we can't use their
# targets.
FetchContent_Populate(pbrt)
set(pbrt_api_dir ${pbrt_SOURCE_DIR}/src/core)

string(TIMESTAMP t2 "%s")
math(EXPR elapsed "${t2} - ${t1}")
message(VERBOSE "FetchContent(pbrt) took ${elapsed} seconds.")

# Use an OBJECT library to avoid circular dependency between the pbrt code
# that wants to call Error and Warning, but we want to provide the implementation
# of those functions in pbrtApi so that they can delegate warnings and errors
# to the interface.
add_library( pbrtParser OBJECT
  ${pbrt_api_dir}/api.h
  ${pbrt_api_dir}/error.h
  ${pbrt_api_dir}/fileutil.cpp
  ${pbrt_api_dir}/fileutil.h
  ${pbrt_api_dir}/floatfile.cpp
  ${pbrt_api_dir}/floatfile.h
  ${pbrt_api_dir}/geometry.h
  ${pbrt_api_dir}/interaction.h
  ${pbrt_api_dir}/material.h
  ${pbrt_api_dir}/medium.h
  ${pbrt_api_dir}/memory.cpp
  ${pbrt_api_dir}/memory.h
  ${pbrt_api_dir}/parallel.cpp
  ${pbrt_api_dir}/parallel.h
  ${pbrt_api_dir}/paramset.cpp
  ${pbrt_api_dir}/paramset.h
  ${pbrt_api_dir}/parser.cpp
  ${pbrt_api_dir}/parser.h
  ${pbrt_api_dir}/pbrt.h
  ${pbrt_api_dir}/quaternion.cpp
  ${pbrt_api_dir}/quaternion.h
  ${pbrt_api_dir}/spectrum.cpp
  ${pbrt_api_dir}/spectrum.h
  ${pbrt_api_dir}/stats.cpp
  ${pbrt_api_dir}/stats.h
  ${pbrt_api_dir}/stringprint.h
  ${pbrt_api_dir}/texture.h
  ${pbrt_api_dir}/transform.cpp
  ${pbrt_api_dir}/transform.h
  ${pbrt_SOURCE_DIR}/src/textures/constant.h
  )
source_group("Source Files" REGULAR_EXPRESSION "[^/]*\\.cpp")
source_group("Header Files" REGULAR_EXPRESSION "[^/]*\\.h")
source_group("Header Files/core" REGULAR_EXPRESSION "core/[^/]*\\.h")
source_group("Header Files/textures" REGULAR_EXPRESSION "textures/[^/]*\\.h")

set_property(TARGET pbrtParser PROPERTY FOLDER "ThirdParty")

if( WIN32 )
  string( REPLACE "/WX" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
else()
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w" )
endif()

target_include_directories( pbrtParser PUBLIC ${pbrt_api_dir} ${pbrt_SOURCE_DIR}/src )
target_compile_definitions( pbrtParser PRIVATE $<$<CONFIG:Release>:NDEBUG> )
target_link_libraries( pbrtParser PUBLIC glog )

###########################################################################
# Annoying compiler-specific details

if(CMAKE_COMPILER_IS_GNUCXX)
  target_compile_options( pbrtParser PRIVATE "-std=gnu++11" )
  target_compile_options( pbrtParser PRIVATE "-Wno-conversion-null")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options( pbrtParser PRIVATE "-Wno-deprecated-register")
endif()

if(MSVC)
  target_compile_definitions( pbrtParser PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

check_include_files( alloca.h HAVE_ALLOCA_H )
if( HAVE_ALLOCA_H )
  target_compile_definitions( pbrtParser PRIVATE PBRT_HAVE_ALLOCA_H )
endif()

check_include_files( memory.h HAVE_MEMORY_H )
if( HAVE_MEMORY_H )
  target_compile_definitions( pbrtParser PRIVATE PBRT_HAVE_MEMORY_H )
endif()

########################################
# thread-local variables

check_cxx_source_compiles( "
#ifdef __CYGWIN__
// Hack to work around https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64697
#error \"No thread_local on cygwin\"
#endif  // __CYGWIN__
thread_local int x; int main() { }
" HAVE_THREAD_LOCAL )

check_cxx_source_compiles( "
__declspec(thread) int x; int main() { }
" HAVE_DECLSPEC_THREAD )

check_cxx_source_compiles( "
__thread int x; int main() { }
" HAVE___THREAD )

if( HAVE_THREAD_LOCAL )
  target_compile_definitions( pbrtParser PRIVATE PBRT_THREAD_LOCAL=thread_local )
elseif( HAVE___THREAD )
  target_compile_definitions( pbrtParser PRIVATE PBRT_THREAD_LOCAL=__thread )
elseif( HAVE_DECLSPEC_THREAD )
  target_compile_definitions( pbrtParser PRIVATE "PBRT_THREAD_LOCAL=__declspec(thread)" )
else()
  message( SEND_ERROR "Unable to find a way to declare a thread-local variable" )
endif()

########################################
# Aligned memory allocation

check_cxx_source_compiles( "
#include <malloc.h>
int main() { void * ptr = _aligned_malloc(1024, 32); }
" HAVE__ALIGNED_MALLOC )

check_cxx_source_compiles( "
#include <stdlib.h>
int main() {
  void *ptr;
  posix_memalign(&ptr, 32, 1024);
} " HAVE_POSIX_MEMALIGN )

check_cxx_source_compiles( "
#include <malloc.h>
int main() {
    void *ptr = memalign(32, 1024);
} " HAVE_MEMALIGN )

if( HAVE__ALIGNED_MALLOC )
  target_compile_definitions( pbrtParser PRIVATE PBRT_HAVE__ALIGNED_MALLOC )
elseif( HAVE_POSIX_MEMALIGN )
  target_compile_definitions( pbrtParser PRIVATE PBRT_HAVE_POSIX_MEMALIGN )
elseif( HAVE_MEMALIGN )
  target_compile_definitions( pbrtParser PRIVATE PBRTHAVE_MEMALIGN )
else()
  message( SEND_ERROR "Unable to find a way to allocate aligned memory" )
endif()

###########################################################################
# Check for various C++11 features and set preprocessor variables or
# define workarounds.

check_cxx_source_compiles(
  "int main() { constexpr int x = 0; }"
  HAVE_CONSTEXPR )
if( HAVE_CONSTEXPR )
  target_compile_definitions( pbrtParser PUBLIC PBRT_CONSTEXPR=constexpr )
else()
  target_compile_definitions( pbrtParser PUBLIC PBRT_CONSTEXPR=const )
endif()
