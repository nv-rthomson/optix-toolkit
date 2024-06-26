#
# Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

if( TARGET glfw )
    return()
endif()

if(OTK_USE_VCPKG)
    find_package(glfw3 CONFIG REQUIRED)
    return()
endif()

function(glfw_folders)
  foreach(_target glfw update_mappings)
    if(TARGET ${_target})
      set_property(TARGET ${_target} PROPERTY FOLDER ThirdParty/GLFW3)
    endif()
  endforeach()
endfunction()

if( NOT OTK_FETCH_CONTENT )
    find_package( glfw3 REQUIRED )
    glfw_folders()
    return()
endif()

set( GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" )
set( GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" )
set( GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" )
set( GLFW_INSTALL OFF CACHE BOOL "Generate GLFW installation target")

include(FetchContent)

message(VERBOSE "Finding glfw3...")
FetchContent_Declare(
    glfw3
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.7
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS
    )
FetchContent_MakeAvailable(glfw3)
# Let find_package know we have it
set(glfw3_FOUND ON PARENT_SCOPE)

glfw_folders()
