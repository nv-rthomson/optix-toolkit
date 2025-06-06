# SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

include(GNUInstallDirs)
include(BuildConfig)
include(FetchGlfw)
include(FetchGlad)
include(embed_cuda)

if(NOT TARGET CUDA::cuda_driver)
  find_package( CUDAToolkit 11.1 REQUIRED )
endif()

embed_cuda(
  CONST HEADER DemandTextureViewerKernelCuda.h
  OUTPUT_TARGET
    demandTextureViewerKernel
  FOLDER
    Examples/DemandLoading
  LIBRARIES
    OptiXToolkit::DemandLoading 
    OptiXToolkit::OTKAppBase
    OptiXToolkit::ShaderUtil
  SOURCES
    DemandTextureViewer.cu
)

otk_add_executable( demandTextureViewer
  DemandTextureViewer.cpp
  )

target_link_libraries( demandTextureViewer
  demandTextureViewerKernel
  OptiXToolkit::DemandLoading
  OptiXToolkit::OTKAppBase
  OptiXToolkit::Gui
  OptiXToolkit::ImageSources
  OptiXToolkit::Util
  CUDA::cudart
  )

set_target_properties( demandTextureViewer PROPERTIES
  FOLDER Examples/DemandLoading
  INSTALL_RPATH ${OptiXToolkit_DIR}/../../OptiXToolkit )

# Copy shared libraries that the built executable depends on.
if( $<TARGET_RUNTIME_DLLS:demandTextureViewer> )
  add_custom_command( TARGET demandTextureViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:demandTextureViewer> $<TARGET_FILE_DIR:demandTextureViewer>
    COMMAND_EXPAND_LISTS )
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
