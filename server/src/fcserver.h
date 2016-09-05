/*
 * Open Pixel Control server for Fadecandy
 *
 * Copyright (c) 2013 Micah Elizabeth Scott
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "rapidjson/document.h"
#include "opc.h"
#include "tcpnetserver.h"
#include "usbdevice.h"
#include <sstream>
#include <vector>

#ifndef __ANDROID__
#include <libusb.h>
#else
#include "jni.h"
#endif //__ANDROID__

#include "tinythread.h"


class FCServer
{
public:
    typedef rapidjson::Value Value;
    typedef rapidjson::Document Document;

#ifdef __ANDROID__
    FCServer();
    int init(const char *);
    bool start();
#else
    bool start(libusb_context *usb);
    void mainLoop();

    FCServer(rapidjson::Document &config);
#endif //__ANDROID__

    const char *errorText() const { return mError.str().c_str(); }
    bool hasError() const { return !mError.str().empty(); }

#ifdef __ANDROID__

    void usbDeviceArrived(int vendorId, int productId, const char * serialNumber, int fileDescriptor);
    void usbDeviceLeft(int fileDescriptor);

    void close();

    static jclass localServiceClass;
    static jclass globalServiceClass;
    static JavaVM* jvm;
    static JNIEnv* env;

#endif //__ANDROID__

private:
    std::ostringstream mError;

#ifndef __ANDROID__
    const Document& mConfig;
    const Value& mListen;
    const Value& mColor;
    const Value& mDevices;
#else
    Document mConfig;
    Value mColor;
    Value mDevices;
#endif //__ANDROID__

    bool mVerbose;
    bool mPollForDevicesOnce;

    TcpNetServer mTcpNetServer;
    tthread::recursive_mutex mEventMutex;
    tthread::thread *mUSBHotplugThread;

    std::vector<USBDevice*> mUSBDevices;

#ifndef __ANDROID__
    struct libusb_context *mUSB;
#endif //__ANDROID__

    static void cbOpcMessage(OPC::Message &msg, void *context);
    static void cbJsonMessage(libwebsocket *wsi, rapidjson::Document &message, void *context);

#ifndef __ANDROID__
    static LIBUSB_CALL int cbHotplug(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);

    bool startUSB(libusb_context *usb);
    void usbDeviceArrived(libusb_device *device);
    void usbDeviceLeft(libusb_device *device);
#endif //__ANDROID__

    void usbDeviceLeft(std::vector<USBDevice*>::iterator iter);

#ifndef __ANDROID__
    bool usbHotplugPoll();
    static void usbHotplugThreadFunc(void *arg);
#endif //__ANDROID__

    // JSON event broadcasters
    void jsonConnectedDevicesChanged();

    // JSON message handlers
    void jsonListConnectedDevices(rapidjson::Document &message);
    void jsonServerInfo(rapidjson::Document &message);
    void jsonDeviceMessage(rapidjson::Document &message);
};
