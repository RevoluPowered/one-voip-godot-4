# The MIT License (MIT)
#
# Copyright (c) 2015 xuan9, 2024 faintbeep
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Adapted from https://github.com/xuan9/Opus-Android


ROOT := $(call my-dir)

# Build libopus
LOCAL_PATH			:= $(ROOT)/opus
include $(CLEAR_VARS)
#include the .mk files
include $(LOCAL_PATH)/celt_sources.mk
include $(LOCAL_PATH)/silk_sources.mk
include $(LOCAL_PATH)/opus_sources.mk

LOCAL_MODULE        := opus

#floating point sources
SILK_SOURCES += $(SILK_SOURCES_FLOAT)
OPUS_SOURCES += $(OPUS_SOURCES_FLOAT)

#ARM build
#CELT_SOURCES += $(CELT_SOURCES_ARM)
#SILK_SOURCES += $(SILK_SOURCES_ARM)
LOCAL_SRC_FILES     := \
$(CELT_SOURCES) $(SILK_SOURCES) $(OPUS_SOURCES)

LOCAL_LDLIBS        := -lm -llog

LOCAL_C_INCLUDES    := \
$(LOCAL_PATH)/include \
$(LOCAL_PATH)/silk \
$(LOCAL_PATH)/silk/float \
$(LOCAL_PATH)/celt

LOCAL_CFLAGS        := -DNULL=0 -DSOCKLEN_T=socklen_t -DLOCALE_NOT_USED -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64
LOCAL_CFLAGS        += -Drestrict='' -D__EMX__ -DOPUS_BUILD -DUSE_ALLOCA -DHAVE_LRINT -DHAVE_LRINTF -O3 -fno-math-errno
LOCAL_CPPFLAGS      := -DBSD=1
LOCAL_CPPFLAGS      += -ffast-math -O3 -funroll-loops

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := opus-share
#LOCAL_SRC_FILES := libopus.a
LOCAL_STATIC_LIBRARIES := opus
include $(BUILD_SHARED_LIBRARY)

