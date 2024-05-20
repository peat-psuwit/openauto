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

#include <gst/gst.h>
#include <gst/gstdebugutils.h>

#include <QApplication>
#include <QResizeEvent>
#include <QRunnable>
#include <QQuickItem>
#include <QStandardPaths>

#include <f1x/openauto/autoapp/Projection/QuickGstVideoOutput.hpp>
#include <f1x/openauto/Common/Log.hpp>

namespace f1x
{
namespace openauto
{
namespace autoapp
{
namespace projection
{

QuickGstVideoOutput::QuickGstVideoOutput(configuration::IConfiguration::Pointer configuration)
    : VideoOutput(std::move(configuration))
    , videoItem_(nullptr)
    , onGstMessageHandlerId(0)
{
    this->moveToThread(QApplication::instance()->thread());
    connect(this, &QuickGstVideoOutput::startPlayback, this, &QuickGstVideoOutput::onStartPlayback, Qt::QueuedConnection);
    connect(this, &QuickGstVideoOutput::stopPlayback, this, &QuickGstVideoOutput::onStopPlayback, Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "createVideoOutput", Qt::BlockingQueuedConnection);
}

void QuickGstVideoOutput::createVideoOutput()
{
    g_autoptr(GError) error = nullptr;

    OPENAUTO_LOG(debug) << "[QuickGstVideoOutput] create.";

    gst_init(nullptr, nullptr);

    gstPipeline_.reset(GST_PIPELINE(gst_parse_launch_full(
        "appsrc name=appsrc ! decodebin3 ! glupload ! glcolorconvert ! "
        "qmlglsink name=qmlglsink force-aspect-ratio=false",
        /* parse context */ nullptr, GST_PARSE_FLAG_FATAL_ERRORS, &error)));
    if (!gstPipeline_ || error) {
        OPENAUTO_LOG(error) << "[QuickGstVideoOutput] fails to create Gst pipeline: "
                            << (error ? error->message : "(unknown error)");
        return;
    }

    // Because Qt on Android doesn't run GLib main loop, we have to receive
    // error from GstBus synchronously.
    {
        std::unique_ptr<GstBus, GObjectDeleter> bus(gst_pipeline_get_bus(gstPipeline_.get()));
        gst_bus_enable_sync_message_emission(bus.get());
        onGstMessageHandlerId = g_signal_connect(bus.get(), "sync-message",
            G_CALLBACK(&QuickGstVideoOutput::onGstMessage), this);
    }

    appsrc_.reset(GST_APP_SRC(gst_bin_get_by_name(GST_BIN(gstPipeline_.get()), "appsrc")));
    qmlglsink_.reset(gst_bin_get_by_name(GST_BIN(gstPipeline_.get()), "qmlglsink"));

    if (!appsrc_ || !qmlglsink_) {
        OPENAUTO_LOG(error) << "[QuickGstVideoOutput] cannot get Gst elements from pipeline?";
        return;
    }

    quickView_ = new QQuickView();
    quickView_->setResizeMode(QQuickView::SizeRootObjectToView);
    quickView_->setSource(QUrl("qrc:/QuickGstVideoOutput.qml"));
    videoItem_ = quickView_->rootObject()->findChild<QQuickItem *>("videoItem");
    if (!videoItem_) {
        OPENAUTO_LOG(error) << "[QuickGstVideoOutput] cannot get videoItem from quickView?";
        return;
    }

    g_object_set(qmlglsink_.get(), "widget", videoItem_, NULL);

    // "The container takes over ownership of window."
    widgetWrapper_.reset(QWidget::createWindowContainer(quickView_));
}

QuickGstVideoOutput::~QuickGstVideoOutput()
{
}

bool QuickGstVideoOutput::open()
{
    return true;
}

bool QuickGstVideoOutput::init()
{
    emit startPlayback();
    return true;
}

void QuickGstVideoOutput::write(uint64_t timestamp, const aasdk::common::DataConstBuffer& buffer)
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

void QuickGstVideoOutput::stop()
{
    emit stopPlayback();
}

void QuickGstVideoOutput::onStartPlayback()
{
    if (!quickView_ || !gstPipeline_)
        return;

    // TODO: a configuration to show fullscren or maximized.
    // TODO: we may have to do this inside createVideoOutput() right away
    // if we want to support automatic video geometry detection.
    widgetWrapper_->showFullScreen();
    quickView_->scheduleRenderJob(QRunnable::create([this]() {
        gst_element_set_state(
            GST_ELEMENT(gstPipeline_.get()), GST_STATE_PLAYING);
    }), QQuickWindow::BeforeSynchronizingStage);
}

void QuickGstVideoOutput::onStopPlayback()
{
    if (!quickView_ || !gstPipeline_)
        return;

    widgetWrapper_->hide();
    gst_element_set_state(GST_ELEMENT(gstPipeline_.get()), GST_STATE_NULL);

    std::unique_ptr<GstBus, GObjectDeleter> bus(gst_pipeline_get_bus(gstPipeline_.get()));
    gst_bus_disable_sync_message_emission(bus.get());
    g_signal_handler_disconnect(bus.get(), onGstMessageHandlerId);
    onGstMessageHandlerId = 0;
}

// Called from whatever thread emitting this message. Right now we only log the
// error, but for any serious handling we'll probably want to post a message to
// Qt main thread.
// TODO: more intelligent handling of error other than logging?
// static
void QuickGstVideoOutput::onGstMessage(GstBus *, GstMessage * message, gpointer user_data)
{
    auto self = static_cast<QuickGstVideoOutput *>(user_data);

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        g_autoptr(GError) err = nullptr;
        g_autofree gchar * dbg_info = nullptr;

        gst_message_parse_error(message, &err, &dbg_info);

        OPENAUTO_LOG(error) << "[QuickGstVideoOutput] Gstreamer error from "
                            << GST_OBJECT_NAME(message->src)
                            << ": " << err->message;
        OPENAUTO_LOG(debug) << "[QuickGstVideoOutput] debug info: "
                            << (dbg_info ? dbg_info : "(none)");
        break;
    }
#if 0
    case GST_MESSAGE_STATE_CHANGED: {
        GstState newstate;
        g_autofree gchar * dump = nullptr;

        if (message->src != GST_OBJECT(self->gstPipeline_.get()))
            break;

        gst_message_parse_state_changed(message, nullptr, &newstate, nullptr);
        dump = gst_debug_bin_to_dot_data(GST_BIN(self->gstPipeline_.get()), GST_DEBUG_GRAPH_SHOW_ALL);

        QFile dumpFile(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/" +
                       gst_element_state_get_name(newstate) + ".dot");
        OPENAUTO_LOG(debug) << "[QuickGstVideoOutput] dump pipeline to " << dumpFile.fileName();
        dumpFile.open(QIODevice::WriteOnly | QIODevice::Text);
        dumpFile.write(dump);

        break;
    }
#endif
    default:
        break;
    }
}

}
}
}
}

#endif
