/*
*  This file is part of openauto project.
*  Copyright (C) 2018 f1x.studio (Michal Szwaj)
*  Copyright (C) 2023 Ratchanan Srirattanamet <peathot@hotmail.com>
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

#ifdef __ANDROID__

#include <boost/asio.hpp>
#include <list>

#include <QAndroidJniObject>
#include <QObject>
#include <private/qjnihelpers_p.h>

#include <aasdk/USB/IUSBHub.hpp>
#include <aasdk/USB/IAccessoryModeQueryChainFactory.hpp>


namespace aasdk
{
namespace usb
{
class IUSBWrapper;

class AndroidUSBHub: public QObject, public IUSBHub,
                     public QtAndroidPrivate::NewIntentListener,
                     public std::enable_shared_from_this<AndroidUSBHub>,
                     boost::noncopyable
{
    Q_OBJECT
public:
    AndroidUSBHub(IUSBWrapper& usbWrapper, boost::asio::io_service& ioService, IAccessoryModeQueryChainFactory& queryChainFactory);

    void start(Promise::Pointer promise) override;
    void cancel() override;

    bool handleNewIntent(JNIEnv *env, jobject intent) override;
    
private:
    typedef std::list<IAccessoryModeQueryChain::Pointer> QueryChainQueue;
    using std::enable_shared_from_this<AndroidUSBHub>::shared_from_this;
    void handleDevice(QAndroidJniObject usbDevice);
    bool isAOAPDevice(const libusb_device_descriptor& deviceDescriptor) const;

    void handleLaunchIntent();
    bool handleIntent(QAndroidJniObject intent);

    IUSBWrapper& usbWrapper_;
    boost::asio::io_service::strand strand_;
    IAccessoryModeQueryChainFactory& queryChainFactory_;
    Promise::Pointer hotplugPromise_;
    QueryChainQueue queryChainQueue_;

    QAndroidJniObject usbManager_;

    bool launchIntentHandled;

    static constexpr uint16_t cGoogleVendorId = 0x18D1;
    static constexpr uint16_t cAOAPId = 0x2D00;
    static constexpr uint16_t cAOAPWithAdbId = 0x2D01;
};

}
}

#endif
