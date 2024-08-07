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

include(CTest)
include(FetchGtest)
include(GoogleTest)

source_group( "CUDA Files" REGULAR_EXPRESSION ".+\\.cu$" )

otk_add_library(DemandGeometryMocks STATIC
  include/OptiXToolkit/DemandGeometry/Mocks/Matchers.h
  include/OptiXToolkit/DemandGeometry/Mocks/MockDemandLoader.h
  include/OptiXToolkit/DemandGeometry/Mocks/MockOptix.h
  include/OptiXToolkit/DemandGeometry/Mocks/OptixCompare.h
  MockOptix.cpp
)
if(MSVC)
  # error C1128: number of sections exceeded object file format limit: compile with /bigobj
  target_compile_options(DemandGeometryMocks PUBLIC $<$<COMPILE_LANGUAGE:CXX>:/bigobj>)
endif()
target_include_directories(DemandGeometryMocks PUBLIC include)
target_link_libraries(DemandGeometryMocks PUBLIC OptiXToolkit::Error OptiXToolkit::Memory OptiX::OptiX GTest::gmock CUDA::cuda_driver)
set_target_properties(DemandGeometryMocks PROPERTIES FOLDER DemandLoading/Tests)
add_library(OptiXToolkit::DemandGeometry::Mocks ALIAS DemandGeometryMocks)

otk_add_executable(TestDemandGeometryMocks
  TestMatchers.cpp
  TestOptixCompare.cpp
)
if(MSVC)
  # Disable unused formal parameter warning in gmock.
  target_compile_options(TestDemandGeometryMocks PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/wd4100>)
endif()
target_link_libraries( TestDemandGeometryMocks PRIVATE OptiXToolkit::DemandGeometry::Mocks OptiXToolkit::Error OptiXToolkit::ShaderUtil OptiXToolkit::DemandGeometry GTest::gmock_main )
set_target_properties(TestDemandGeometryMocks PROPERTIES 
  CXX_STANDARD 14  # Required by latest gtest
  FOLDER DemandLoading/Tests
  )
gtest_discover_tests(TestDemandGeometryMocks PROPERTIES LABELS DemandLoading)

otk_add_executable( TestDemandGeometry
  LaunchIntersectAabb.h
  LaunchIntersectAabb.cu
  TestIntersectAabb.cpp
  TestProxyInstances.cpp
)
if(MSVC)
  # Disable unused formal parameter warning in gmock.
  target_compile_options(TestDemandGeometry PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/wd4100>)
endif()
target_link_libraries( TestDemandGeometry PRIVATE OptiXToolkit::DemandGeometry::Mocks OptiXToolkit::Error OptiXToolkit::ShaderUtil OptiXToolkit::DemandGeometry GTest::gmock_main )
set_target_properties(TestDemandGeometry PROPERTIES 
  CXX_STANDARD 14  # Required by latest gtest
  FOLDER DemandLoading/Tests
  )
gtest_discover_tests(TestDemandGeometry PROPERTIES LABELS DemandLoading)
