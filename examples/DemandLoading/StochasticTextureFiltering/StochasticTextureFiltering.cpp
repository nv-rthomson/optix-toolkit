//
// Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

// This include is needed to avoid a link error
#include <optix_stubs.h>

#include <StochasticTextureFilteringKernel.h>

#include <OptiXToolkit/DemandTextureAppBase/DemandTextureApp.h>
#include <OptiXToolkit/ImageSources/MultiCheckerImage.h>

#include <OptiXToolkit/ShaderUtil/ray_cone.h>
#include <OptiXToolkit/ShaderUtil/vec_math.h>
#include <OptiXToolkit/Gui/Gui.h>
#include <OptiXToolkit/Gui/glfw3.h>

#include <OptiXToolkit/Error/cudaErrorCheck.h>
#include <OptiXToolkit/Error/optixErrorCheck.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ShapeMaker.h"
#include "StochasticTextureFilteringParams.h"

#include <optix_stubs.h>

using namespace otk;
using namespace demandTextureApp;
using namespace demandLoading;
using namespace imageSource;

//------------------------------------------------------------------------------
// StochasticTextureFilteringApp
//------------------------------------------------------------------------------

class StochasticTextureFilteringApp : public DemandTextureApp
{
  public:
    StochasticTextureFilteringApp( const char* appTitle, unsigned int width, unsigned int height, const std::string& outFileName, bool glInterop );
    void setTextureName( const char* textureName ) { m_textureName = textureName; }
    void createTexture() override;
    void initView() override;
    void createScene();
    void setSceneId( int sceneId ) { m_sceneId = sceneId; }
    void initLaunchParams( PerDeviceOptixState& state, unsigned int numDevices ) override;
    
    void buildAccel( PerDeviceOptixState& state ) override;
    void createSBT( PerDeviceOptixState& state ) override;
    void drawGui() override;

  protected:
    std::string m_textureName;
    int m_sceneId = 0;

    std::vector<float4> m_vertices;
    std::vector<float3> m_normals;
    std::vector<float2> m_tex_coords;
    std::vector<uint32_t> m_material_indices;
    std::vector<TriangleHitGroupData> m_materials;
    
    SurfaceTexture makeSurfaceTex( int kd, int kdtex, int ks, int kstex, int kt, int kttex, float roughness, float ior );
    void addShapeToScene( std::vector<Vert>& shape, unsigned int materialId );
    void copyGeometryToDevice();

    void cursorPosCallback( GLFWwindow* window, double xpos, double ypos ) override;
    void mouseButtonCallback( GLFWwindow* window, int button, int action, int mods ) override;

    void keyCallback( GLFWwindow* window, int32_t key, int32_t scancode, int32_t action, int32_t mods ) override;
    void pollKeys() override;

    int m_minRayDepth = 0;
    int m_maxRayDepth = 6;
    int m_updateRayCones = 1;

    // GUI controls
    unsigned int m_selectedOutputValueId = 0;
    unsigned int m_selectedPixelFilterId = 0;
    unsigned int m_selectedTextureFilterId = 0;
    unsigned int m_selectedTextureJitterId = 0;
    bool         m_singleSample = false;
    float        m_filterWidth = 1.0f;
    float        m_filterStrength = 1.0f;
};


StochasticTextureFilteringApp::StochasticTextureFilteringApp( const char* appTitle, unsigned int width, unsigned int height, const std::string& outFileName, bool glInterop )
    : DemandTextureApp( appTitle, width, height, outFileName, glInterop )
{
    m_backgroundColor = float4{1.0f, 1.0f, 1.0f, 0.0f};
    m_projection = Projection::PINHOLE;
}


void StochasticTextureFilteringApp::initView()
{
    if( m_sceneId == 0 )
        setView( float3{0.0f, 0.0f, 1.0f}, float3{-10.0f, -5.0f, 0.0f}, float3{0.0f, 0.0f, 1.0f}, 30.0f );
    else if( m_sceneId == 1 )
        setView( float3{0.0f, 40.0f, 5.0f}, float3{0.0f, 0.0f, 0.0f}, float3{0.0f, 0.0f, 1.0f}, 30.0f );
    else 
        setView( float3{0.0f, 25.0f, 7.0f}, float3{0.0f, 0.0f, 3.0f}, float3{0.0f, 0.0f, 1.0f}, 30.0f );
}


void StochasticTextureFilteringApp::buildAccel( PerDeviceOptixState& state )
{
    // Copy vertex data to device
    void* d_vertices = nullptr;
    const size_t vertices_size_bytes = m_vertices.size() * sizeof( float4 );
    OTK_ERROR_CHECK( cudaMalloc( &d_vertices, vertices_size_bytes ) );
    OTK_ERROR_CHECK( cudaMemcpy( d_vertices, m_vertices.data(), vertices_size_bytes, cudaMemcpyHostToDevice ) );
    state.d_vertices = reinterpret_cast<CUdeviceptr>( d_vertices );

    // Copy material indices to device
    void* d_material_indices = nullptr;
    const size_t material_indices_size_bytes = m_material_indices.size() * sizeof( uint32_t );
    OTK_ERROR_CHECK( cudaMalloc( &d_material_indices, material_indices_size_bytes ) );
    OTK_ERROR_CHECK( cudaMemcpy( d_material_indices, m_material_indices.data(), material_indices_size_bytes, cudaMemcpyHostToDevice ) );

    // Make triangle input flags (one per sbt record).  Here, we are just disabling the anyHit programs
    std::vector<uint32_t> triangle_input_flags( m_materials.size(), OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT );

    // Make GAS accel build inputs
    OptixBuildInput triangle_input                           = {};
    triangle_input.type                                      = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
    triangle_input.triangleArray.vertexFormat                = OPTIX_VERTEX_FORMAT_FLOAT3; 
    triangle_input.triangleArray.vertexStrideInBytes         = static_cast<uint32_t>( sizeof( float4 ) );
    triangle_input.triangleArray.numVertices                 = static_cast<uint32_t>( m_vertices.size() );
    triangle_input.triangleArray.vertexBuffers               = &state.d_vertices;
    triangle_input.triangleArray.flags                       = triangle_input_flags.data();
    triangle_input.triangleArray.numSbtRecords               = static_cast<uint32_t>( triangle_input_flags.size() );
    triangle_input.triangleArray.sbtIndexOffsetBuffer        = reinterpret_cast<CUdeviceptr>( d_material_indices );
    triangle_input.triangleArray.sbtIndexOffsetSizeInBytes   = sizeof( uint32_t );
    triangle_input.triangleArray.sbtIndexOffsetStrideInBytes = sizeof( uint32_t );

    // Make accel options
    OptixAccelBuildOptions accel_options = {};
    accel_options.buildFlags             = OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
    accel_options.operation              = OPTIX_BUILD_OPERATION_BUILD;

    // Compute memory usage for accel build
    OptixAccelBufferSizes gas_buffer_sizes;
    const unsigned int num_build_inputs = 1;
    OTK_ERROR_CHECK( optixAccelComputeMemoryUsage( state.context, &accel_options, &triangle_input, num_build_inputs, &gas_buffer_sizes ) );

    // Allocate temporary buffer needed for accel build
    void* d_temp_buffer = nullptr;
    OTK_ERROR_CHECK( cudaMalloc( &d_temp_buffer, gas_buffer_sizes.tempSizeInBytes ) );

    // Allocate output buffer for (non-compacted) accel build result, and also compactedSize property.
    void* d_buffer_temp_output_gas_and_compacted_size = nullptr;
    size_t      compactedSizeOffset = roundUp<size_t>( gas_buffer_sizes.outputSizeInBytes, 8ull );
    OTK_ERROR_CHECK( cudaMalloc( &d_buffer_temp_output_gas_and_compacted_size, compactedSizeOffset + 8 ) );

    // Set up the accel build to return the compacted size, so compaction can be run after the build
    OptixAccelEmitDesc emitProperty = {};
    emitProperty.type               = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
    emitProperty.result             = ( CUdeviceptr )( (char*)d_buffer_temp_output_gas_and_compacted_size + compactedSizeOffset );

    // Finally perform the accel build
    OTK_ERROR_CHECK( optixAccelBuild(
                state.context,
                CUstream{0},
                &accel_options,
                &triangle_input,
                num_build_inputs,                    
                reinterpret_cast<CUdeviceptr>( d_temp_buffer ),
                gas_buffer_sizes.tempSizeInBytes,
                reinterpret_cast<CUdeviceptr>( d_buffer_temp_output_gas_and_compacted_size ),
                gas_buffer_sizes.outputSizeInBytes,
                &state.gas_handle,
                &emitProperty,
                1
                ) );

    // Delete temporary buffers used for the accel build
    OTK_ERROR_CHECK( cudaFree( d_temp_buffer ) );
    OTK_ERROR_CHECK( cudaFree( d_material_indices ) );

    // Copy the size of the compacted GAS accel back from the device
    size_t compacted_gas_size;
    OTK_ERROR_CHECK( cudaMemcpy( &compacted_gas_size, (void*)emitProperty.result, sizeof(size_t), cudaMemcpyDeviceToHost ) );

    // If compaction reduces the size of the accel, copy to a new buffer and delete the old one
    if( compacted_gas_size < gas_buffer_sizes.outputSizeInBytes )
    {
        OTK_ERROR_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.d_gas_output_buffer ), compacted_gas_size ) );
        // use handle as input and output
        OTK_ERROR_CHECK( optixAccelCompact( state.context, 0, state.gas_handle, state.d_gas_output_buffer, compacted_gas_size, &state.gas_handle ) );
        OTK_ERROR_CHECK( cudaFree( (void*)d_buffer_temp_output_gas_and_compacted_size ) );
    }
    else
    {
        state.d_gas_output_buffer = reinterpret_cast<CUdeviceptr>( d_buffer_temp_output_gas_and_compacted_size );
    }
}


void  StochasticTextureFilteringApp::createSBT( PerDeviceOptixState& state )
{
    // Raygen record 
    void*  d_raygen_record = nullptr;
    const size_t raygen_record_size = sizeof( RayGenSbtRecord );
    OTK_ERROR_CHECK( cudaMalloc( &d_raygen_record, raygen_record_size ) );
    RayGenSbtRecord raygen_record = {};
    OTK_ERROR_CHECK( optixSbtRecordPackHeader( state.raygen_prog_group, &raygen_record ) );
    OTK_ERROR_CHECK( cudaMemcpy( d_raygen_record, &raygen_record, raygen_record_size, cudaMemcpyHostToDevice ) );

    // Miss record
    void* d_miss_record = nullptr;
    const size_t miss_record_size = sizeof( MissSbtRecord );
    OTK_ERROR_CHECK( cudaMalloc( &d_miss_record, miss_record_size ) );
    MissSbtRecord miss_record;
    OTK_ERROR_CHECK( optixSbtRecordPackHeader( state.miss_prog_group, &miss_record ) );
    miss_record.data.background_color = m_backgroundColor;
    OTK_ERROR_CHECK( cudaMemcpy( d_miss_record, &miss_record, miss_record_size, cudaMemcpyHostToDevice ) );

    // Hitgroup records (one for each material)
    const unsigned int MAT_COUNT = static_cast<unsigned int>( m_materials.size() );
    void* d_hitgroup_records = nullptr;
    const size_t hitgroup_record_size = sizeof( TriangleHitGroupSbtRecord );
    OTK_ERROR_CHECK( cudaMalloc( &d_hitgroup_records, hitgroup_record_size * MAT_COUNT ) );
    std::vector<TriangleHitGroupSbtRecord> hitgroup_records( MAT_COUNT );
    for( unsigned int mat_idx = 0; mat_idx < MAT_COUNT; ++mat_idx )
    {
        OTK_ERROR_CHECK( optixSbtRecordPackHeader( state.hitgroup_prog_group, &hitgroup_records[mat_idx] ) );
        TriangleHitGroupData* hg_data = &hitgroup_records[mat_idx].data;
        // Copy material definition, and then fill in device-specific values for vertices, normals, tex_coords
        *hg_data = m_materials[mat_idx];
        hg_data->vertices = reinterpret_cast<float4*>( state.d_vertices );
        hg_data->normals = state.d_normals;
        hg_data->tex_coords = state.d_tex_coords;
    }
    OTK_ERROR_CHECK( cudaMemcpy( d_hitgroup_records, &hitgroup_records[0], hitgroup_record_size * MAT_COUNT, cudaMemcpyHostToDevice ) );

    // Set up SBT
    state.sbt.raygenRecord                = reinterpret_cast<CUdeviceptr>( d_raygen_record );
    state.sbt.missRecordBase              = reinterpret_cast<CUdeviceptr>( d_miss_record );
    state.sbt.missRecordStrideInBytes     = static_cast<uint32_t>( miss_record_size );
    state.sbt.missRecordCount             = 1;
    state.sbt.hitgroupRecordBase          = reinterpret_cast<CUdeviceptr>( d_hitgroup_records );
    state.sbt.hitgroupRecordStrideInBytes = static_cast<uint32_t>( hitgroup_record_size );
    state.sbt.hitgroupRecordCount         = MAT_COUNT;
}

void StochasticTextureFilteringApp::initLaunchParams( PerDeviceOptixState& state, unsigned int numDevices )
{
    // If the GUI state has changed, reset the subframe id.
    if( state.params.i[PIXEL_FILTER_ID] != static_cast<int>( m_selectedPixelFilterId ) ||
        state.params.i[TEXTURE_FILTER_ID] != static_cast<int>( m_selectedTextureFilterId ) ||
        state.params.i[TEXTURE_JITTER_ID] != static_cast<int>( m_selectedTextureJitterId ) ||
        state.params.f[TEXTURE_FILTER_WIDTH_ID] != m_filterWidth ||
        state.params.f[TEXTURE_FILTER_STRENGTH_ID] != m_filterStrength ||
        m_singleSample )
        m_subframeId = 0;

    DemandTextureApp::initLaunchParams( state, numDevices );

    state.params.i[SUBFRAME_ID]       = m_subframeId;
    state.params.i[PIXEL_FILTER_ID]   = m_selectedPixelFilterId; 
    state.params.i[TEXTURE_FILTER_ID] = m_selectedTextureFilterId;
    state.params.i[TEXTURE_JITTER_ID] = m_selectedTextureJitterId;
    state.params.i[MOUSEX_ID]         = static_cast<int>( m_mousePrevX );
    state.params.i[MOUSEY_ID]         = static_cast<int>( m_mousePrevY );

    state.params.f[MIP_SCALE_ID]               = m_mipScale;
    state.params.f[TEXTURE_FILTER_WIDTH_ID]    = m_filterWidth;
    state.params.f[TEXTURE_FILTER_STRENGTH_ID] = m_filterStrength;
}

void StochasticTextureFilteringApp::createTexture()
{
    std::shared_ptr<ImageSource> imageSource( createExrImage( m_textureName.c_str() ) );
    if( !imageSource && !m_textureName.empty() )
        std::cout << "ERROR: Could not find image " << m_textureName << ". Substituting procedural image.\n";
    if( !imageSource )
        imageSource.reset( new imageSources::MultiCheckerImage<uchar4>( 16384, 16384, 256, true, false ) );
    
    demandLoading::TextureDescriptor texDesc0 = makeTextureDescriptor( CU_TR_ADDRESS_MODE_CLAMP, FILTER_POINT );
    demandLoading::TextureDescriptor texDesc1 = makeTextureDescriptor( CU_TR_ADDRESS_MODE_CLAMP, FILTER_BILINEAR );

    for( PerDeviceOptixState& state : m_perDeviceOptixStates )
    {
        OTK_ERROR_CHECK( cudaSetDevice( state.device_idx ) );
        const demandLoading::DemandTexture& texture = state.demandLoader->createTexture( imageSource, texDesc0 );
        if( m_textureIds.empty() )
            m_textureIds.push_back( texture.getId() );
        state.demandLoader->createTexture( imageSource, texDesc1 );
    }
}

void StochasticTextureFilteringApp::createScene()
{
    const unsigned int NUM_SEGMENTS = 128;
    TriangleHitGroupData mat{};
    std::vector<Vert> shape;

    // ground plane
    if( m_sceneId == 0 )
    {
        mat.tex = makeSurfaceTex( 0xeeeeee, 0, 0x010101, -1, 0x000000, -1, 0.1f, 0.0f );
        m_materials.push_back( mat );
        ShapeMaker::makeAxisPlane( float3{-100, -100, 0}, float3{100, 100, 0}, shape );
        addShapeToScene( shape, m_materials.size() - 1 );
    }

    // square
    if( m_sceneId == 1 )
    {
        mat.tex = makeSurfaceTex( 0xeeeeee, 0, 0x010101, -1, 0x000000, -1, 0.1f, 0.0f );
        m_materials.push_back( mat );
        ShapeMaker::makeAxisPlane( float3{-10, 0, -10}, float3{10, 0, 10}, shape );
        addShapeToScene( shape, m_materials.size() - 1 );
    }

    // vase
    if( m_sceneId == 2 )
    {
        // Ground
        mat.tex = makeSurfaceTex( 0x777777, -1, 0x777777, -1, 0x000000, -1, 0.01, 0.0f );
        m_materials.push_back( mat );

        ShapeMaker::makeAxisPlane( float3{-40, -40, 0}, float3{40, 40, 0}, shape );
        addShapeToScene( shape, m_materials.size() - 1 );

        // Vase
        mat.tex = makeSurfaceTex( 0xffffff, 0, 0x252525, -1, 0x000000, -1, 0.0001, 0.0f );
        m_materials.push_back( mat );

        ShapeMaker::makeVase( float3{0.0f, 0.0f, 0.01f}, 1.0f, 4.0f, 8.0f, NUM_SEGMENTS, shape );
        addShapeToScene( shape, m_materials.size() -1 );

        ShapeMaker::makeSphere( float3{-5.0f, 1.0f, 0.7f}, 0.7f, NUM_SEGMENTS, shape );
        addShapeToScene( shape, m_materials.size() - 1 );

        // Vase liners with diffuse material to block negative curvature traps
        mat.tex = makeSurfaceTex( 0x111111, -1, 0x111111, -1, 0x000000, -1, 0.1, 0.0f );
        m_materials.push_back( mat );

        ShapeMaker::makeVase( float3{0.0f, 0.0f, 0.01f}, 0.99f, 3.99f, 8.0f, NUM_SEGMENTS, shape );
        addShapeToScene( shape, m_materials.size() -1 );
    }

    copyGeometryToDevice();
}

SurfaceTexture StochasticTextureFilteringApp::makeSurfaceTex( int kd, int kdtex, int ks, int kstex, int kt, int kttex, float roughness, float ior )
{
    SurfaceTexture tex;
    tex.emission     = ColorTex{ float3{ 0.0f, 0.0f, 0.0f }, -1 };
    tex.diffuse      = ColorTex{ float3{ ((kd>>16)&0xff)/255.0f, ((kd>>8)&0xff)/255.0f, ((kd>>0)&0xff)/255.0f }, kdtex };
    tex.specular     = ColorTex{ float3{ ((ks>>16)&0xff)/255.0f, ((ks>>8)&0xff)/255.0f, ((ks>>0)&0xff)/255.0f }, kstex };
    tex.transmission = ColorTex{ float3{ ((kt>>16)&0xff)/255.0f, ((kt>>8)&0xff)/255.0f, ((kt>>0)&0xff)/255.0f }, kttex };
    tex.roughness    = roughness;
    tex.ior          = ior;
    return tex;
}

void StochasticTextureFilteringApp::addShapeToScene( std::vector<Vert>& shape, unsigned int materialId )
{
    for( unsigned int i=0; i<shape.size(); ++i )
    {
        m_vertices.push_back( make_float4( shape[i].p ) );
        m_normals.push_back( shape[i].n );
        m_tex_coords.push_back( shape[i].t );
        if( i % 3 == 0 )
            m_material_indices.push_back( materialId );
    }
}

void StochasticTextureFilteringApp::copyGeometryToDevice()
{
    for( PerDeviceOptixState& state : m_perDeviceOptixStates )
    {
        OTK_ERROR_CHECK( cudaSetDevice( state.device_idx ) );
        
        // m_vertices copied in buildAccel
        // m_material_indices copied in createSBT
        // m_materials copied in createSBT

        // m_normals
        OTK_ERROR_CHECK( cudaMalloc( &state.d_normals, m_normals.size() * sizeof(float3) ) );
        OTK_ERROR_CHECK( cudaMemcpy( state.d_normals, m_normals.data(),  m_normals.size() * sizeof(float3), cudaMemcpyHostToDevice ) );

        // m_tex_coords
        OTK_ERROR_CHECK( cudaMalloc( &state.d_tex_coords, m_tex_coords.size() * sizeof(float2) ) );
        OTK_ERROR_CHECK( cudaMemcpy( state.d_tex_coords, m_tex_coords.data(),  m_tex_coords.size() * sizeof(float2), cudaMemcpyHostToDevice ) );
    }
}

void StochasticTextureFilteringApp::cursorPosCallback( GLFWwindow* /*window*/, double xpos, double ypos )
{
    float dx = static_cast<float>( xpos - m_mousePrevX );
    float dy = static_cast<float>( ypos - m_mousePrevY );
    m_mousePrevX = xpos;
    m_mousePrevY = ypos;

    if( m_mouseButton < 0 )
        return;

    const float pan = 0.005f * length(m_camera.lookAt() - m_camera.eye());
    const float rot = 0.002f;

    float3 U, V, W;
    m_camera.UVWFrame( U, V, W );
    V.z = 0.0f;
    
    if( m_mouseButton == GLFW_MOUSE_BUTTON_LEFT )  
        panCamera( ( pan * dx * normalize(U) ) + ( -pan * dy * normalize(V) ) );
    else if( m_mouseButton == GLFW_MOUSE_BUTTON_RIGHT )  
        rotateCamera( -rot * dx );

    m_subframeId = 0;
}

void StochasticTextureFilteringApp::mouseButtonCallback( GLFWwindow* window, int button, int action, int /*mods*/ )
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent( button, (bool) action );

    if( !io.WantCaptureMouse )
    {
        glfwGetCursorPos( window, &m_mousePrevX, &m_mousePrevY );
        m_mouseButton = ( action == GLFW_PRESS ) ? button : NO_BUTTON;
    }
}

void StochasticTextureFilteringApp::keyCallback( GLFWwindow* window, int32_t key, int32_t /*scancode*/, int32_t action, int32_t /*mods*/ )
{
    if( action != GLFW_PRESS )
        return;

    if( key == GLFW_KEY_ESCAPE ) {
        glfwSetWindowShouldClose( window, true );
    } else if( key == GLFW_KEY_C ) {
        initView();
    } else if( key >= GLFW_KEY_1 && key <= GLFW_KEY_9 ) {
        m_render_mode = key - GLFW_KEY_1;
    } else if( key == GLFW_KEY_LEFT ) {
        m_maxRayDepth = std::max( m_maxRayDepth - 1, 1 );
    } else if( key == GLFW_KEY_RIGHT ) {
        m_maxRayDepth++;
    } else if( key == GLFW_KEY_UP ) {
        m_minRayDepth = std::min( m_minRayDepth + 1, m_maxRayDepth );
    } else if( key == GLFW_KEY_DOWN ) {
        m_minRayDepth = std::max( m_minRayDepth - 1, 0 );
    } else if( key == GLFW_KEY_EQUAL ) {
        m_mipScale *= 0.5f;
    } else if( key == GLFW_KEY_MINUS ) {
        m_mipScale *= 2.0f;
    } else if( key == GLFW_KEY_X ) {
        for( PerDeviceOptixState& state : m_perDeviceOptixStates )
        {
            OTK_ERROR_CHECK( cudaSetDevice( state.device_idx ) );
            state.demandLoader->unloadTextureTiles( m_textureIds[0] );
        }
    } else if( key == GLFW_KEY_U ) {
        m_updateRayCones = static_cast<int>( !m_updateRayCones );
    } else if( key == GLFW_KEY_P && m_projection == Projection::PINHOLE ) {
        m_projection = Projection::THINLENS;
    } else if( key == GLFW_KEY_P && m_projection == Projection::THINLENS ) {
        m_projection = Projection::PINHOLE;
    } else if( key == GLFW_KEY_O ) {
        m_lens_width *= 1.1f;
    } else if ( key == GLFW_KEY_I ) {
        m_lens_width /= 1.1f;
    } else if( key == GLFW_KEY_F1 ) {
        saveImage();
    }

    m_subframeId = 0;
}

void StochasticTextureFilteringApp::pollKeys()
{
    const float pan = 0.04f;
    const float vpan = 0.01f;
    const float rot = 0.003f;

    float3 U, V, W;
    m_camera.UVWFrame( U, V, W );

    if( glfwGetKey( getWindow(), GLFW_KEY_A ) )
        panCamera( normalize(U) * -pan );
    if( glfwGetKey( getWindow(), GLFW_KEY_D ) )
        panCamera( normalize(U) * pan );
    if( glfwGetKey( getWindow(), GLFW_KEY_S ) )
        panCamera( normalize( float3{V.x, V.y, 0.0f} ) * -pan );
    if( glfwGetKey( getWindow(), GLFW_KEY_W ) )
        panCamera( normalize( float3{V.x, V.y, 0.0f} ) * pan );
    if( glfwGetKey( getWindow(), GLFW_KEY_Q ) )
        panCamera( float3{0.0f, 0.0f, -vpan} );
    if( glfwGetKey( getWindow(), GLFW_KEY_E ) )
        panCamera( float3{0.0f, 0.0f, vpan} );
    if( glfwGetKey( getWindow(), GLFW_KEY_J ) )
        rotateCamera( rot );
    if( glfwGetKey( getWindow(), GLFW_KEY_L ) )
        rotateCamera( -rot );
}

void displayComboBox( const char* title, const char* items[], unsigned int numItems, unsigned int& selectedId )
{
    if( ImGui::BeginCombo( title, items[selectedId], ImGuiComboFlags_HeightLarge ) )
    {
        for( unsigned int id = 0; id < numItems; ++id )
        {
            bool isSelected = ( selectedId == id ); 
            if( ImGui::Selectable( items[id], selectedId ) )
                selectedId = id;
            if( isSelected )
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void StochasticTextureFilteringApp::drawGui()
{
    otk::beginFrameImGui();

    ImGui::SetNextWindowPos( ImVec2( 5, 5 ) );
    ImGui::SetNextWindowSize( ImVec2( 0, 0 ) );
    ImGui::SetNextWindowBgAlpha( 0.75f );
    ImGui::Begin( "" );

    ImGui::Text("framerate: %.1f fps", ImGui::GetIO().Framerate);
    displayComboBox( "Pixel Filter", PIXEL_FILTER_MODE_NAMES, pfSIZE, m_selectedPixelFilterId );
    displayComboBox( "Filter Mode", TEXTURE_FILTER_MODE_NAMES, fmSIZE, m_selectedTextureFilterId );
    displayComboBox( "Jitter Kernel", TEXTURE_JITTER_MODE_NAMES, jmSIZE, m_selectedTextureJitterId );
    ImGui::Checkbox( "Single Sample", &m_singleSample );

    ImGui::SliderFloat("Filter Width", &m_filterWidth, 0.0f, 3.0f, "%.3f", 0);
    ImGui::SliderFloat("Filter Strength", &m_filterStrength, 0.0f, 3.0f, "%.3f", 0);

    otk::endFrameImGui();
}


//------------------------------------------------------------------------------
// Main function
//------------------------------------------------------------------------------

void printUsage( const char* argv0 )
{
    std::cerr << "\n\nUsage: " << argv0 << " [options]\n\n";
    std::cout << "Options:  --scene [0-5], --texture <texturefile.exr>, --launches <numLaunches>\n";
    std::cout << "          --dim=<width>x<height>, --file <outputfile.ppm>, --no-gl-interop\n\n";
    std::cout << "Mouse:    <LMB>:          pan camera\n";
    std::cout << "          <RMB>:          rotate camera\n\n";
    std::cout << "Keyboard: <ESC>:          exit\n";
    std::cout << "          1-7:            set output variable\n";
    std::cout << "          <LEFT>,<RIGHT>: change max depth\n";
    std::cout << "          <UP>,<DOWN>:    change min depth\n";
    std::cout << "          WASD,QE:        pan camera\n";
    std::cout << "          J,L:            rotate camera\n";
    std::cout << "          C:              reset view\n";
    std::cout << "          +,-:            change mip bias\n";
    std::cout << "          P:              toggle thin lens camera\n";
    std::cout << "          U:              toggle distance-based vs. ray cones\n";
    std::cout << "          I,O:            change lens width\n";
    std::cout << "          X:              unload all texture tiles\n\n";
}

int main( int argc, char* argv[] )
{
    int         windowWidth  = 900;
    int         windowHeight = 600;
    const char* textureName  = "";
    const char* outFileName  = "";
    bool        glInterop    = true;
    int         numLaunches  = 256;
    int         sceneId      = 1;

    printUsage( argv[0] );

    for( int i = 1; i < argc; ++i )
    {
        const std::string arg( argv[i] );
        const bool        lastArg = ( i == argc - 1 );

        if( ( arg == "--texture" ) && !lastArg )
            textureName = argv[++i];
        else if( ( arg == "--file" ) && !lastArg )
            outFileName = argv[++i];
        else if( arg.substr( 0, 6 ) == "--dim=" )
            otk::parseDimensions( arg.substr( 6 ).c_str(), windowWidth, windowHeight );
        else if( arg == "--no-gl-interop" )
            glInterop = false;
        else if( arg == "--launches" && !lastArg )
            numLaunches = atoi( argv[++i] );
        else if( arg == "--scene" && !lastArg )
            sceneId = atoi( argv[++i] );
        else 
            exit(0);
    }

    StochasticTextureFilteringApp app( "Stochastic Texture Filtering", windowWidth, windowHeight, outFileName, glInterop );
    app.setSceneId( sceneId );
    app.initView();
    app.setNumLaunches( numLaunches );
    app.sceneIsTriangles( true );
    app.initDemandLoading();
    app.setTextureName( textureName );
    app.createTexture();
    app.createScene();
    app.resetAccumulator();
    app.initOptixPipelines( StochasticTextureFilteringCudaText(), StochasticTextureFilteringCudaSize );
    app.startLaunchLoop();
    app.printDemandLoadingStats();
    
    return 0;
}
