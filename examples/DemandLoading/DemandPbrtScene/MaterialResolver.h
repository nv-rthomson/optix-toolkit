#pragma once

#include "Dependencies.h"
#include "MaterialResolverStatistics.h"

#include <cuda.h>

namespace demandPbrtScene {

class FrameStopwatch;

struct Options;
struct SceneGeometry;
struct SceneSyncState;

enum class MaterialResolution
{
    NONE    = 0,
    PARTIAL = 1,
    FULL    = 2,
};

inline bool operator<( MaterialResolution lhs, MaterialResolution rhs )
{
    return static_cast<int>( lhs ) < static_cast<int>( rhs );
}

class MaterialResolver
{
  public:
    virtual ~MaterialResolver() = default;

    virtual void resolveOneMaterial() = 0;

    /// Returns true if proxy material is fully resolved and IAS needs to be rebuilt
    virtual bool resolveMaterialForGeometry( uint_t proxyGeomId, SceneGeometry& geom, SceneSyncState& syncState ) = 0;

    virtual MaterialResolution resolveRequestedProxyMaterials( CUstream              stream,
                                                               const FrameStopwatch& frameTime,
                                                               SceneSyncState&       syncState ) = 0;

    virtual MaterialResolverStats getStatistics() const = 0;
};

MaterialResolverPtr createMaterialResolver( const Options&        options,
                                            MaterialLoaderPtr     materialLoader,
                                            DemandTextureCachePtr demandTextureCache,
                                            ProgramGroupsPtr      programGroups );

}  // namespace demandPbrtScene
