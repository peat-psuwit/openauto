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

#include <memory>

#include <QObject>
#include <QQuickView>
#include <QWidget>

#include <gst/gstpipeline.h>
#include <gst/app/gstappsrc.h>

#include <boost/noncopyable.hpp>

#include <f1x/openauto/autoapp/Projection/VideoOutput.hpp>

namespace {
    struct GObjectDeleter {
        void operator()(gpointer obj) {
            g_object_unref(obj);
        }
    };
}

namespace f1x
{
namespace openauto
{
namespace autoapp
{
namespace projection
{

class QuickGstVideoOutput: public QObject, public VideoOutput, boost::noncopyable
{
    Q_OBJECT

public:
    QuickGstVideoOutput(configuration::IConfiguration::Pointer configuration);
    ~QuickGstVideoOutput();
    bool open() override;
    bool init() override;
    void write(uint64_t timestamp, const aasdk::common::DataConstBuffer& buffer) override;
    void stop() override;

signals:
    void startPlayback();
    void stopPlayback();

protected slots:
    void createVideoOutput();
    void onStartPlayback();
    void onStopPlayback();

private:
    static void onGstMessage(GstBus * bus, GstMessage * message, gpointer user_data);

    std::unique_ptr<GstPipeline, GObjectDeleter> gstPipeline_;
    std::unique_ptr<QWidget> widgetWrapper_;

    std::unique_ptr<GstAppSrc, GObjectDeleter> appsrc_;
    std::unique_ptr<GstElement, GObjectDeleter> qmlglsink_;

    /* Owned by widgetWrapper_ */
    QQuickView * quickView_;
    /* Owned by quickView_ */
    QQuickItem * videoItem_;

    gulong onGstMessageHandlerId;
};

}
}
}
}

#endif
