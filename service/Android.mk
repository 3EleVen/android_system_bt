#
#  Copyright (C) 2015 Google
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at:
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := uuid_test.cpp uuid.cpp
LOCAL_CFLAGS += -std=c++11
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := uuid_test_bd
include $(BUILD_HOST_NATIVE_TEST)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	a2dp_source.cpp \
	core_stack.cpp \
	gatt_server.cpp \
	host.cpp \
	logging_helpers.cpp \
	main.cpp \
	uuid.cpp

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../

LOCAL_CFLAGS += -std=c++11
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := bthost
LOCAL_REQUIRED_MODULES = bluetooth.default
LOCAL_SHARED_LIBRARIES += \
	libchrome \
	libcutils \
	libhardware \
	liblog

include $(BUILD_EXECUTABLE)
