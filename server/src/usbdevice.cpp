/*
 * Abstract base class for USB-attached devices.
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

#include "libwebsockets.h"   // Lazy portable way to get gettimeofday()
#include "usbdevice.h"
#include <iostream>

#ifdef __ANDROID__
#include "android/log.h"
#define APP_NAME "fadecandy-server"
#include "jni.h"
#include "fcserver.h"
#endif //__ANDROID__

#ifdef __ANDROID__
USBDevice::USBDevice(const char *type, bool verbose, int fileDescriptor, const char * serial)
  : mTypeString(type),
    mSerialString(strdup(serial)),
    mVerbose(verbose),
    mFD(fileDescriptor)
#else
USBDevice::USBDevice(libusb_device * device, const char *type, bool verbose)
  : mDevice(libusb_ref_device(device)),
    mHandle(0),
    mTypeString(type),
    mSerialString(0),
    mVerbose(verbose)
#endif //__ANDROID__
{
  gettimeofday(&mTimestamp, NULL);
}

USBDevice::~USBDevice()
{
#ifndef __ANDROID__
  if (mHandle) {
    libusb_close(mHandle);
  }
  if (mDevice) {
    libusb_unref_device(mDevice);
  }
#endif //__ANDROID__
}

#ifdef __ANDROID__
void USBDevice::android_bulk_transfer(void *buffer, int length)
{

  uint8_t *data = (uint8_t*) buffer;

  if (FCServer::jvm != 0) {

    int getEnvStat = FCServer::jvm->GetEnv((void **)&FCServer::env, JNI_VERSION_1_6);

    if (getEnvStat == JNI_EDETACHED) {

      if (FCServer::jvm->AttachCurrentThread(&FCServer::env, NULL) != 0) {

        __android_log_print(ANDROID_LOG_ERROR, APP_NAME, "failed to attach\n");
      }
    } else if (getEnvStat == JNI_EVERSION) {

      __android_log_print(ANDROID_LOG_ERROR, APP_NAME, "jni: version not supported\n");
    }
  }
  else {
    __android_log_print(ANDROID_LOG_ERROR, APP_NAME, "jvm not defined\n");
  }

  jint fd = getFileDescriptor();
  jbyteArray data_ba = FCServer::env->NewByteArray(length);
  FCServer::env->SetByteArrayRegion( data_ba, 0, length, (const jbyte*)data );

  jmethodID methodId = FCServer::env->GetMethodID(FCServer::globalServiceClass, "bulkTransfer", "(II[B)I");
  FCServer::env->CallIntMethod(FCServer::localServiceClass, methodId, fd, 2000, data_ba);

  FCServer::env->DeleteLocalRef(data_ba);
}
#endif //__ANDROID__

bool USBDevice::probeAfterOpening()
{
  // By default, any device is supported by the time we get to opening it.
  return true;
}

void USBDevice::writeColorCorrection(const Value &color)
{
  // Optional. By default, ignore color correction messages.
}

bool USBDevice::matchConfiguration(const Value &config)
{
  if (!config.IsObject()) {
    return false;
  }

  const Value &vtype = config["type"];
  const Value &vserial = config["serial"];

  if (!vtype.IsNull() && (!vtype.IsString() || strcmp(vtype.GetString(), mTypeString))) {
    return false;
  }

  if (mSerialString && !vserial.IsNull() &&
      (!vserial.IsString() || strcmp(vserial.GetString(), mSerialString))) {
    return false;
  }

  return true;
}

const USBDevice::Value *USBDevice::findConfigMap(const Value &config)
{
  const Value &vmap = config["map"];

  if (vmap.IsArray()) {
    // The map is optional, but if it exists it needs to be an array.
    return &vmap;
  }

  if (!vmap.IsNull() && mVerbose) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_VERBOSE, APP_NAME, "Device configuration 'map' must be an array.\n");
#else
    std::clog << "Device configuration 'map' must be an array.\n";
#endif //__ANDROID__
  }

  return 0;
}

void USBDevice::writeMessage(Document &msg)
{
  const char *type = msg["type"].GetString();

  if (!strcmp(type, "device_color_correction")) {
    // Single-device color correction
    writeColorCorrection(msg["color"]);
    return;
  }

  msg.AddMember("error", "Unknown device-specific message type", msg.GetAllocator());
}

void USBDevice::describe(rapidjson::Value &object, Allocator &alloc)
{
  object.AddMember("type", mTypeString, alloc);
  if (mSerialString) {
    object.AddMember("serial", mSerialString, alloc);
  }

  /*
   * The connection timestamp lets a particular connection instance be identified
   * reliably, even if the same device connects and disconnects.
   *
   * We encode the timestamp as 64-bit millisecond count, so we don't have to worry about
   * the portability of string/float conversions. This also matches a common JS format.
   */

  uint64_t timestamp = (uint64_t)mTimestamp.tv_sec * 1000 + mTimestamp.tv_usec / 1000;
  object.AddMember("timestamp", timestamp, alloc);
}
