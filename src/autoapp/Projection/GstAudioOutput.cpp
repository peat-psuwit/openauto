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

#ifdef USE_GSTREAMER

#include <f1x/openauto/autoapp/Projection/GstAudioOutput.hpp>
#include <f1x/openauto/Common/Log.hpp>

namespace f1x
{
namespace openauto
{
namespace autoapp
{
namespace projection
{

GstAudioOutput::GstAudioOutput(uint32_t channelCount, uint32_t sampleSize, uint32_t sampleRate)
{
    g_autoptr(GError) error = nullptr;

    OPENAUTO_LOG(debug) << "[GstAudioOutput] create";

    gst_audio_info_set_format(&audioInfo_,
        gst_audio_format_build_integer(
            /* sign */ true, G_LITTLE_ENDIAN, sampleSize, sampleSize),
        sampleRate, channelCount, /* GstAudioChannelPosition[] */ nullptr);

    gstPipeline_.reset(GST_PIPELINE(gst_parse_launch_full(
        "appsrc name=appsrc ! audioconvert ! audioresample ! autoaudiosink",
        /* parse context */ nullptr, GST_PARSE_FLAG_FATAL_ERRORS, &error)));
    if (!gstPipeline_ || error) {
        OPENAUTO_LOG(error) << "[GstAudioOutput] fails to create Gst pipeline: "
                            << (error ? error->message : "(unknown error)");
        return;
    }

    appsrc_.reset(GST_APP_SRC(gst_bin_get_by_name(GST_BIN(gstPipeline_.get()), "appsrc")));
    if (!appsrc_) {
        OPENAUTO_LOG(error) << "[GstAudioOutput] cannot get Gst elements from pipeline?";
        return;
    }

    g_autoptr(GstCaps) caps = gst_audio_info_to_caps(&audioInfo_);
    gst_app_src_set_caps(appsrc_.get(), caps);
}

bool GstAudioOutput::open()
{
    if (!gstPipeline_)
        return false;
    
    auto stateChangeRet = gst_element_set_state(GST_ELEMENT(gstPipeline_.get()), GST_STATE_PAUSED);
    return stateChangeRet != GST_STATE_CHANGE_FAILURE;
}

void GstAudioOutput::write(aasdk::messenger::Timestamp::ValueType, const aasdk::common::DataConstBuffer& buffer)
{
    if (!appsrc_)
        return;

    auto gstBuffer = gst_buffer_new_memdup(buffer.cdata, buffer.size);

    // The document seems to infer that it's safe to push the buffer from
    // arbitrary thread.
    // FIXME: it's not clear if we have to wait for the appsrc to be in certain
    // state before we can start pushing data.
    gst_app_src_push_buffer(appsrc_.get(), gstBuffer);
}

void GstAudioOutput::start()
{
    if (!gstPipeline_)
        return;

    gst_element_set_state(GST_ELEMENT(gstPipeline_.get()), GST_STATE_PLAYING);
}

void GstAudioOutput::stop()
{
    if (!gstPipeline_)
        return;

    gst_element_set_state(GST_ELEMENT(gstPipeline_.get()), GST_STATE_NULL);
}

void GstAudioOutput::suspend()
{
    if (!gstPipeline_)
        return;

    gst_element_set_state(GST_ELEMENT(gstPipeline_.get()), GST_STATE_PAUSED);
}

uint32_t GstAudioOutput::getSampleSize() const
{
    return audioInfo_.finfo->width;
}

uint32_t GstAudioOutput::getChannelCount() const
{
    return audioInfo_.channels;
}

uint32_t GstAudioOutput::getSampleRate() const
{
    return audioInfo_.rate;
}

}
}
}
}

#endif
