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

#ifdef __ANDROID__

#include <QtAndroid>
#include <QAndroidJniEnvironment>
#include <QMetaObject>

#include <f1x/aasdk_android/AndroidUSBHub.hpp>
#include <f1x/openauto/Common/Log.hpp>

/* TODO: Maybe move all Android interaction to a mockable class? */

namespace aasdk
{
namespace usb
{

AndroidUSBHub::AndroidUSBHub(IUSBWrapper& usbWrapper, boost::asio::io_service& ioService, IAccessoryModeQueryChainFactory& queryChainFactory)
    : usbWrapper_(usbWrapper)
    , strand_(ioService)
    , queryChainFactory_(queryChainFactory)
    , launchIntentHandled(false)
{
    auto activity = QtAndroid::androidActivity();
    if (!activity.isValid())
    {
        OPENAUTO_LOG(error) << "[AndroidUSBHub] cannot get Android Activity. USB will not work.";
        return;
    }

    auto USB_SERVICE = QAndroidJniObject::getStaticObjectField<jstring>(
        "android/content/Context", "USB_SERVICE");
    usbManager_ = activity.callObjectMethod(
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
        USB_SERVICE.object<jstring>());
}

void AndroidUSBHub::start(Promise::Pointer promise)
{
    if (!usbManager_.isValid())
    {
        OPENAUTO_LOG(error) << "[AndroidUSBHub] cannot get USBManager. USB will not work.";
        strand_.dispatch([promise = std::move(promise)]() {
            promise->reject(error::Error(error::ErrorCode::USB_LIST_DEVICES));
        });
        return;
    }

    strand_.dispatch([this, self = this->shared_from_this(), promise = std::move(promise)]() {
        if(hotplugPromise_ != nullptr)
        {
            hotplugPromise_->reject(error::Error(error::ErrorCode::OPERATION_ABORTED));
            hotplugPromise_.reset();
        }

        hotplugPromise_ = std::move(promise);

        if (!launchIntentHandled) {
            // I'm not sure if we can get intent and stuffs from an arbitrary
            // thread. So let's post the task over to the Qt thread which will read
            // the intent for us and then dispatch a task back if it finds a device.
            //
            // Even then, I'm not sure if we can do this from Qt thread, or we
            // have to post this over to Android thread. We'll see...
            QMetaObject::invokeMethod(this, &AndroidUSBHub::handleLaunchIntent,
                Qt::QueuedConnection);

            launchIntentHandled = true;
        }

        QtAndroidPrivate::registerNewIntentListener(this);
    });
}

void AndroidUSBHub::cancel()
{
    strand_.dispatch([this, self = this->shared_from_this()]() mutable {
        if(hotplugPromise_ != nullptr)
        {
            hotplugPromise_->reject(error::Error(error::ErrorCode::OPERATION_ABORTED));
            hotplugPromise_.reset();
        }

        std::for_each(queryChainQueue_.begin(), queryChainQueue_.end(), std::bind(&IAccessoryModeQueryChain::cancel, std::placeholders::_1));

        QtAndroidPrivate::unregisterNewIntentListener(this);
    });
}

void AndroidUSBHub::handleLaunchIntent()
{
    auto activity = QtAndroid::androidActivity();
    auto intent = activity.callObjectMethod("getIntent", "()Landroid/content/Intent;");
    handleIntent(intent);
}

bool AndroidUSBHub::handleNewIntent(JNIEnv *env, jobject intent)
{
    return handleIntent(intent);
}

bool AndroidUSBHub::handleIntent(QAndroidJniObject intent)
{
    auto EXTRA_DEVICE = QAndroidJniObject::getStaticObjectField<jstring>(
        "android.hardware.usb.UsbManager", "EXTRA_DEVICE");
    auto usbDevice = intent.callObjectMethod(
        "getParcelableExtra", "(Ljava/lang/String;)Landroid/os/Parcelable;",
        EXTRA_DEVICE.object<jstring>());
    if (!usbDevice.isValid())
        return false;

    // When a UsbDevice is obtained via an intent filter, we "automatically has
    // permission to access the device until the device is disconnected."
    // https://developer.android.com/develop/connectivity/usb/host#using-intents
    strand_.dispatch([self = this->shared_from_this(), usbDevice] {
        self->handleDevice(usbDevice);
    });
    return true;
}

bool AndroidUSBHub::isAOAPDevice(const libusb_device_descriptor& deviceDescriptor) const
{
    return deviceDescriptor.idVendor == cGoogleVendorId &&
            (deviceDescriptor.idProduct == cAOAPId || deviceDescriptor.idProduct == cAOAPWithAdbId);
}

void AndroidUSBHub::handleDevice(QAndroidJniObject usbDevice)
{
    if(hotplugPromise_ == nullptr)
    {
        return;
    }

    QAndroidJniEnvironment env;
    auto usbDeviceConnection = usbManager_.callObjectMethod(
        "openDevice", "(Landroid/hardware/usb/UsbDevice;)Landroid/hardware/usb/UsbDeviceConnection;",
        usbDevice.object());
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (!usbDeviceConnection.isValid())
    {
        OPENAUTO_LOG(error) << "[AndroidUSBHub] cannot open device from intent?";
        return;
    }
    auto fd = usbDeviceConnection.callMethod<jint>("getFileDescriptor");

    DeviceHandle handle;
    auto wrapResult = usbWrapper_.wrapSysDevice(fd, [usbDeviceConnection]() {
        usbDeviceConnection.callMethod<void>("close");
    }, handle);

    if(wrapResult != 0 || !handle)
    {
        return;
    }

    libusb_device *device = usbWrapper_.getDevice(handle);
    libusb_device_descriptor deviceDescriptor;
    if(usbWrapper_.getDeviceDescriptor(device, deviceDescriptor) != 0)
    {
        return;
    }

    if(this->isAOAPDevice(deviceDescriptor))
    {
        hotplugPromise_->resolve(std::move(handle));
        hotplugPromise_.reset();
    }
    else
    {
        ////////// Workaround for VMware
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        //////////

        queryChainQueue_.emplace_back(queryChainFactory_.create());

        auto queueElementIter = std::prev(queryChainQueue_.end());
        auto queryChainPromise = IAccessoryModeQueryChain::Promise::defer(strand_);
        queryChainPromise->then([this, self = this->shared_from_this(), queueElementIter](DeviceHandle handle) mutable {
                queryChainQueue_.erase(queueElementIter);
            },
            [this, self = this->shared_from_this(), queueElementIter](const error::Error& e) mutable {
                queryChainQueue_.erase(queueElementIter);
            });

        queryChainQueue_.back()->start(std::move(handle), std::move(queryChainPromise));
    }
}



}
}

#endif
