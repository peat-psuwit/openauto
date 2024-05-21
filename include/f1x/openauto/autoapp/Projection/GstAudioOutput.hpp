/*
*  This file is part of openauto project.
*  Copyright (C) 2018 f1x.studio (Michal Szwaj)
*  Copyright (C) 2024 Ratchanan Srirattanamet <peathot@hotmail.com>
*
*  openauto is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 3 of the License, or
*  (at your option) any later version.

*  openauto is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with openauto. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#ifdef USE_GSTREAMER

#include <gst/gst.h>
#include <gst/audio/audio-info.h>
#include <gst/app/gstappsrc.h>

#include <f1x/openauto/autoapp/Projection/GObjectDeleter.hpp>
#include <f1x/openauto/autoapp/Projection/IAudioOutput.hpp>

namespace f1x
{
namespace openauto
{
namespace autoapp
{
namespace projection
{

class GstAudioOutput: public IAudioOutput
{

public:
    GstAudioOutput(uint32_t channelCount, uint32_t sampleSize, uint32_t sampleRate);
    bool open() override;
    void write(aasdk::messenger::Timestamp::ValueType, const aasdk::common::DataConstBuffer& buffer) override;
    void start() override;
    void stop() override;
    void suspend() override;
    uint32_t getSampleSize() const override;
    uint32_t getChannelCount() const override;
    uint32_t getSampleRate() const override;

private:
    GstAudioInfo audioInfo_;
    std::unique_ptr<GstPipeline, GObjectDeleter> gstPipeline_;
    std::unique_ptr<GstAppSrc, GObjectDeleter> appsrc_;
};

}
}
}
}

#endif
