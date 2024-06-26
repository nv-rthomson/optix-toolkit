//
// Copyright (c) 2023 NVIDIA Corporation.  All rights reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and proprietary
// rights in and to this software, related documentation and any modifications thereto.
// Any use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation is strictly
// prohibited.
//
// TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
// AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
// INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
// SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
// LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
// BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
// INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGES
//

#include <OptiXToolkit/PbrtSceneLoader/GoogleLogger.h>

#include <glog/logging.h>

namespace otk {
namespace pbrt {

void GoogleLogger::start( const char* programName )
{
    google::InitGoogleLogging( programName );
    FLAGS_logtostderr = true;
    FLAGS_minloglevel = m_minLogLevel;
}

void GoogleLogger::stop()
{
    google::ShutdownGoogleLogging();
}

void GoogleLogger::info( std::string text, const char *file, int line ) const
{
    if( text.empty() )
        return;
    if( text.back() != '\n' )
        text += '\n';
    google::LogMessage( file, line, google::GLOG_INFO ).stream() << text;
}

void GoogleLogger::warning( std::string text, const char *file, int line ) const
{
    if( text.empty() )
        return;
    if( text.back() != '\n' )
        text += '\n';
    google::LogMessage( file, line, google::GLOG_WARNING ).stream() << text;
}

void GoogleLogger::error( std::string text, const char *file, int line ) const
{
    if( text.empty() )
        return;
    if( text.back() != '\n' )
        text += '\n';
    google::LogMessage( file, line, google::GLOG_ERROR ).stream() << text;
}

}  // namespace pbrt
}  // namespace otk
