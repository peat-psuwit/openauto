/*
*  This file is part of openauto project.
*  Copyright (C) 2018 f1x.studio (Michal Szwaj)
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

#include <QApplication>
#include <f1x/openauto/autoapp/Projection/QtVideoOutput.hpp>
#include <f1x/openauto/Common/Log.hpp>

#ifdef __ANDROID__
// TODO: have our own VideoOutput that use Gstreamer directly.
#include <gst/gst.h>
#include <mutex>

// FIXME: see below
extern "C" void gst_init_static_plugins ();

static void initGstOnce() {
    std::once_flag gst_init_once;
    std::call_once(gst_init_once, []() {
        gst_init(nullptr, nullptr); // Will automatically register static plugins.

        // FIXME: because dynamic lookup of gst_init_static_plugins() happens inside
        // whatever .so ships gst_init() (?), it will look up this in MultimediaGstTools
        // instead of our just-built gstreamer_android.
        // This is also the reason we have to wrap this in call_once.
        gst_init_static_plugins();
    });
}

#endif

namespace f1x
{
namespace openauto
{
namespace autoapp
{
namespace projection
{

QtVideoOutput::QtVideoOutput(configuration::IConfiguration::Pointer configuration)
    : VideoOutput(std::move(configuration))
{
#ifdef __ANDROID__
    initGstOnce();
#endif

    this->moveToThread(QApplication::instance()->thread());
    connect(this, &QtVideoOutput::startPlayback, this, &QtVideoOutput::onStartPlayback, Qt::QueuedConnection);
    connect(this, &QtVideoOutput::stopPlayback, this, &QtVideoOutput::onStopPlayback, Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "createVideoOutput", Qt::BlockingQueuedConnection);
}

void QtVideoOutput::createVideoOutput()
{
    OPENAUTO_LOG(debug) << "[QtVideoOutput] create.";
    videoWidget_ = std::make_unique<QVideoWidget>();
    mediaPlayer_ = std::make_unique<QMediaPlayer>(nullptr, QMediaPlayer::StreamPlayback);
}


bool QtVideoOutput::open()
{
    return videoBuffer_.open(QIODevice::ReadWrite);
}

bool QtVideoOutput::init()
{
    emit startPlayback();
    return true;
}

void QtVideoOutput::stop()
{
    emit stopPlayback();
}

void QtVideoOutput::write(uint64_t, const aasdk::common::DataConstBuffer& buffer)
{
    videoBuffer_.write(reinterpret_cast<const char*>(buffer.cdata), buffer.size);
}

void QtVideoOutput::onStartPlayback()
{
    videoWidget_->setAspectRatioMode(Qt::IgnoreAspectRatio);
    videoWidget_->setFocus();
    //videoWidget_->setWindowFlags(Qt::WindowStaysOnTopHint);
    videoWidget_->setFullScreen(true);
    videoWidget_->show();

    mediaPlayer_->setVideoOutput(videoWidget_.get());
    mediaPlayer_->setMedia(QMediaContent(), &videoBuffer_);
    mediaPlayer_->play();
    OPENAUTO_LOG(debug) << "Player error state -> " << mediaPlayer_->errorString().toStdString();
}

void QtVideoOutput::onStopPlayback()
{
    videoWidget_->hide();
    mediaPlayer_->stop();
}

}
}
}
}
