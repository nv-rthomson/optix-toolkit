// SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
//

#include "testCommon.h"

#include <OptiXToolkit/Error/cudaErrorCheck.h>
#include <OptiXToolkit/Error/optixErrorCheck.h>

#include <optix.h>
#include <optix_function_table_definition.h>
#include <optix_stubs.h>

void TestCommon::SetUp()
{
    // Initialize CUDA runtime
    OTK_ERROR_CHECK( cudaFree( 0 ) );

    // Create optix context

    OptixDeviceContextOptions optixOptions = {};

    OTK_ERROR_CHECK( optixInit() );

    CUcontext cuCtx = nullptr;  // zero means take the current context
    OTK_ERROR_CHECK( optixDeviceContextCreate( cuCtx, &optixOptions, &optixContext ) );
}

void TestCommon::TearDown()
{
    OTK_ERROR_CHECK( optixDeviceContextDestroy( optixContext ) );
}

cuOmmBaking::Result TestCommon::saveImageToFile( std::string imageNamePrefix, const std::vector<uchar3>& image, uint32_t width, uint32_t height )
{
    m_imageNamePrefix = imageNamePrefix;

    try
    {
#ifdef GENERATE_GOLD_IMAGES
        std::string imageName = m_imageNamePrefix + "_gold.ppm";
#else
        std::string imageName = m_imageNamePrefix + ".ppm";
#endif

        ImagePPM ppm( (const void*)image.data(), width, height, IMAGE_PIXEL_FORMAT_UCHAR3 );
        ppm.writePPM( imageName, false );
    }
    catch( ... )
    {
        EXPECT_TRUE( false );
        return cuOmmBaking::Result::ERROR_INTERNAL;
    }

    return cuOmmBaking::Result::SUCCESS;
}

void TestCommon::compareImage()
{
#ifdef GENERATE_GOLD_IMAGES
    EXPECT_TRUE( !"Generating gold image, no test result" );
    return;
#endif
    float tolerance = 2.0f / 255;
    int   numErrors;
    float avgError;
    float maxError;

    std::string imageName     = m_imageNamePrefix + ".ppm";
    std::string goldImageName = TEST_OMM_BAKING_GOLD_DIR + m_imageNamePrefix + "_gold.ppm";

    try
    {
        ImagePPM::compare( imageName, goldImageName, tolerance, numErrors, avgError, maxError );
    }
    catch( ... )
    {
        ASSERT_TRUE( !"Failed to compare with gold image" );
    }

    EXPECT_GT( 32, numErrors );
}
