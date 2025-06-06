// SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
//

#pragma once

#include <OptiXToolkit/Error/cuErrorCheck.h>

#include <cuda.h>

namespace otk {

// DeviceRingBuffer implements a device-side ring buffer.  Allocations
// do not need to be freed, but buffer overflow can be detected by calling
// free after each allocation is done.

const unsigned long long BAD_ALLOC        = 0xFFFFFFFFFFFFFFFFULL;
const unsigned int       WARP_SIZE        = 32;

enum AllocMode
{
    THREAD_BASED, WARP_BASE_POINTER, WARP_NON_INTERLEAVED, WARP_INTERLEAVED
};

#ifdef __CUDACC__
__forceinline__ __device__ unsigned int getLaneId()
{
    unsigned ret;
    asm volatile( "mov.u32 %0, %%laneid;" : "=r"( ret ) );
    return ret;
}

__forceinline__ __device__ unsigned int warpPrefixScan( unsigned int val )
{
    const unsigned int laneId = getLaneId();
    unsigned int sum = 0;

    if( __activemask() == 0xFFFFFFFFU )
    {
        const unsigned int mask = __activemask();
        sum = val;
        unsigned int v = __shfl_up_sync( mask, sum, 1 );
        sum += ( laneId >= 1 ) ? v : 0;
        v = __shfl_up_sync( mask, sum, 2 );
        sum += ( laneId >= 2 ) ? v : 0;
        v = __shfl_up_sync( mask, sum, 4 );
        sum += ( laneId >= 4 ) ? v : 0;
        v = __shfl_up_sync( mask, sum, 8 );
        sum += ( laneId >= 8 ) ? v : 0;
        v = __shfl_up_sync( mask, sum, 16 );
        sum += ( laneId >= 16 ) ? v : 0;
        return sum;
    }

    const unsigned int mask = __activemask();
    for( int i = 0; i < WARP_SIZE; ++i )
    {
        unsigned int v = __shfl_sync( mask, val, i );
        if( ( i <= laneId ) && ( ( mask >> i ) & 1 ) )
            sum += v;
    }

    return sum;
}
#endif

struct DeviceRingBuffer
{
    char*               buffer;     // Backing store for the ring buffer.
    unsigned long long  buffSize;   // Size of the buffer in bytes. must be power of 2.
    unsigned long long* nextStart;  // Offset of the next allocation (% buffSize).
    AllocMode           allocMode;  // Allocation mode
    unsigned int        interleaveBytes;  // interleave bytes for WARP_INTERLEAVE mode

#ifndef __CUDACC__
    /// Initialize the ring buffer
    void init( unsigned long long buffSize_, AllocMode allocMode_, unsigned int interleaveBytes_ = 4 )
    {
        allocMode = allocMode_;
        buffSize  = buffSize_;
        interleaveBytes = interleaveBytes_;
        OTK_ERROR_CHECK( cuMemAlloc( reinterpret_cast<CUdeviceptr*>( &buffer ), buffSize ) );
        OTK_ERROR_CHECK( cuMemAlloc( reinterpret_cast<CUdeviceptr*>( &nextStart ), sizeof( unsigned long long ) ) );
        clear( 0 );
    }

    /// Tear down, freeing all the memory
    void tearDown()
    {
        OTK_ERROR_CHECK( cuMemFree( reinterpret_cast<CUdeviceptr>( buffer ) ) );
        OTK_ERROR_CHECK( cuMemFree( reinterpret_cast<CUdeviceptr>( nextStart ) ) );
    }

    /// Clear the allocator
    void clear( CUstream stream = 0 )
    {
        OTK_ERROR_CHECK( cuMemsetD8Async( reinterpret_cast<CUdeviceptr>( nextStart ), 0, sizeof( unsigned long long ), stream ) );
    }
#endif

#ifdef __CUDACC__
    /// Allocate memory from the ring buffer, returning a pointer to it, and the memory handle.
    __forceinline__ __device__ char* alloc( unsigned long long allocSize, unsigned long long* handle )
    {
        return ( allocMode != THREAD_BASED ) ? allocW( allocSize, handle ) : allocT( allocSize, handle );
    }

    /// Allocate memory from the ring buffer, returning a pointer to it.
    __forceinline__ __device__ char* alloc( unsigned long long allocSize )
    {
        unsigned long long handle;
        return alloc( allocSize, &handle );
    }

    /// Get the pointer for a given handle
    __forceinline__ __device__ char* getPointer( unsigned long long handle )
    {
        return buffer + ( handle & ( buffSize - 1 ) );
    }

    __forceinline__ __device__ int memLeft( unsigned long long handle )
    {
        int rval = (int)( (handle + buffSize) - atomicAdd( nextStart, 0 ) );
        return rval;
    }

    /// Calling free is not required for DeviceRingBuffer, but the call checks to see
    /// if the buffer has overflowed (memory was reallocated before being freed).
    __forceinline__ __device__ bool free( unsigned long long handle )
    {
        if( allocMode != THREAD_BASED )
            return ( ( handle == BAD_ALLOC ) || ( handle + buffSize > atomicAdd( nextStart, 0 ) ) );

        const unsigned int activeMask = __activemask();
        const unsigned int leadLane   = __ffs( activeMask ) - 1;
        const unsigned int laneId     = getLaneId();

        bool rval = 0;
        if( laneId == leadLane )
            rval = ( ( handle == BAD_ALLOC ) || ( handle + buffSize > atomicAdd( nextStart, 0 ) ) );
        rval = __shfl_sync( activeMask, rval, leadLane );
        return rval;
    }

  protected:
    // Implementation of Thread-based memory allocation
    __forceinline__ __device__ char* allocT( unsigned long long allocSize, unsigned long long* handle )
    {
        *handle = atomicAdd( nextStart, allocSize );
        if( ( *handle & ( buffSize - 1 ) ) + allocSize <= buffSize )
            return getPointer( *handle );

        // Previous allocation straddled end of ring buffer, try again
        *handle = atomicAdd( nextStart, allocSize );
        if( ( *handle & ( buffSize - 1 ) ) + allocSize <= buffSize )
            return getPointer( *handle );

        *handle = BAD_ALLOC;
        return nullptr;
    }

    // Implementation of Warp-based memory allocation
    __forceinline__ __device__ char* allocW( unsigned int allocSizeInBytes, unsigned long long* handle )
    {
        const unsigned int activeMask = __activemask();
        const unsigned int lastLane   = 31 - __clz( activeMask );
        const unsigned int laneId     = getLaneId();

        // WARP_INTERLEAVED and WARP_BASE_POINTER
        unsigned int totalAllocSizeInBytes = allocSizeInBytes * WARP_SIZE;
        unsigned int allocOffset = (allocMode == WARP_INTERLEAVED) ? interleaveBytes * laneId : 0;

        if( allocMode == WARP_NON_INTERLEAVED )
        {
            totalAllocSizeInBytes = warpPrefixScan( allocSizeInBytes );
            allocOffset = totalAllocSizeInBytes - allocSizeInBytes;
        }

        // Get the base pointer
        char* ptr = nullptr;
        if( laneId == lastLane )
        {
            ptr = allocT( totalAllocSizeInBytes, handle );
        }
        *handle = __shfl_sync( activeMask, *handle, lastLane );

        // Return the pointer for this lane
        ptr = (char*) __shfl_sync( activeMask, (unsigned long long)ptr, lastLane );
        return ptr + allocOffset;
    }
#endif
};

}  // namespace otk
