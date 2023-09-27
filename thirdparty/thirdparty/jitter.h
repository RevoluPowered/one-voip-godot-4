/**************************************************************************/
/*  jitter.h                                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

/* Copyright (C) 2002 Jean-Marc Valin
   File: speex_jitter.h

   Adaptive jitter buffer for Speex

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/*
TODO:
- Add short-term estimate
- Defensive programming
  + warn when last returned < last desired (begative buffering)
  + warn if update_delay not called between get() and tick() or is called twice in a row
- Linked list structure for holding the packets instead of the current fixed-size array
  + return memory to a pool
  + allow pre-allocation of the pool
  + optional max number of elements
- Statistics
  + drift
  + loss
  + late
  + jitter
  + buffering delay
*/

#ifndef JITTER_H
#define JITTER_H

/** @defgroup JitterBuffer JitterBuffer: Adaptive jitter buffer
 *  This is the jitter buffer that reorders UDP/RTP packets and adjusts the buffer size
 * to maintain good quality and low latency.
 *  @{
 */

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "core/object/ref_counted.h"
#include "core/variant/variant.h"

/** Packet has been retrieved */
#define JITTER_BUFFER_OK 0
/** Packet is lost or is late */
#define JITTER_BUFFER_MISSING 1
/** A "fake" packet is meant to be inserted here to increase buffering */
#define JITTER_BUFFER_INSERTION 2
/** There was an error in the jitter buffer */
#define JITTER_BUFFER_INTERNAL_ERROR -1
/** Invalid argument */
#define JITTER_BUFFER_BAD_ARGUMENT -2

/** Set minimum amount of extra buffering required (margin) */
#define JITTER_BUFFER_SET_MARGIN 0
/** Get minimum amount of extra buffering required (margin) */
#define JITTER_BUFFER_GET_MARGIN 1
/* JITTER_BUFFER_SET_AVAILABLE_COUNT wouldn't make sense */

/** Get the amount of available packets currently buffered */
#define JITTER_BUFFER_GET_AVAILABLE_COUNT 3
/** Included because of an early misspelling (will remove in next release) */
#define JITTER_BUFFER_GET_AVALIABLE_COUNT 3

/** Assign a function to destroy unused packet. When setting that, the jitter
	buffer no longer copies packet data. */
#define JITTER_BUFFER_SET_DESTROY_CALLBACK 4
/**  */
#define JITTER_BUFFER_GET_DESTROY_CALLBACK 5

/** Tell the jitter buffer to only adjust the delay in multiples of the step parameter provided */
#define JITTER_BUFFER_SET_DELAY_STEP 6
/**  */
#define JITTER_BUFFER_GET_DELAY_STEP 7

/** Tell the jitter buffer to only do concealment in multiples of the size parameter provided */
#define JITTER_BUFFER_SET_CONCEALMENT_SIZE 8
#define JITTER_BUFFER_GET_CONCEALMENT_SIZE 9

/** Absolute max amount of loss that can be tolerated regardless of the delay. Typical loss
	should be half of that or less. */
#define JITTER_BUFFER_SET_MAX_LATE_RATE 10
#define JITTER_BUFFER_GET_MAX_LATE_RATE 11

/** Equivalent cost of one percent late packet in timestamp units */
#define JITTER_BUFFER_SET_LATE_COST 12
#define JITTER_BUFFER_GET_LATE_COST 13

#define speex_assert(cond)                           \
	{                                                \
		if (!(cond)) {                               \
			speex_fatal("assertion failed: " #cond); \
		}                                            \
	}

#define SPEEX_JITTER_MAX_BUFFER_SIZE 200 /**< Maximum number of packets in jitter buffer */

#define TSUB(a, b) ((int32_t)((a) - (b)))

#define GT32(a, b) (((int32_t)((a) - (b))) > 0)
#define GE32(a, b) (((int32_t)((a) - (b))) >= 0)
#define LT32(a, b) (((int32_t)((a) - (b))) < 0)
#define LE32(a, b) (((int32_t)((a) - (b))) <= 0)

#define ROUND_DOWN(x, step) ((x) < 0 ? ((x) - (step) + 1) / (step) * (step) : (x) / (step) * (step))

#define MAX_TIMINGS 40
#define MAX_BUFFERS 3
#define TOP_DELAY 40

#include "core/variant/variant.h"

/** Definition of an incoming packet */
class JitterBufferPacket : public RefCounted {
	GDCLASS(JitterBufferPacket, RefCounted);

private:
	PackedByteArray data;
	int64_t timestamp_usec = 0;
	int64_t span = 0;
	int64_t sequence = 0;
	int64_t user_data = 0;

protected:
	static void _bind_methods();

public:
	void set_data(const PackedByteArray &p_data);
	void set_timestamp(int64_t p_timestamp);
	void set_span(int64_t p_span);
	void set_sequence(int64_t p_sequence);
	void set_user_data(int64_t p_user_data);

	PackedByteArray get_data() const;
	int64_t get_timestamp() const;
	int64_t get_span() const;
	int64_t get_sequence() const;
	int64_t get_user_data() const;
};

/** Buffer that keeps the time of arrival of the latest packets */
class TimingBuffer {
	int filled = 0; /**< Number of entries occupied in "timing" and "counts"*/
	int curr_count = 0; /**< Number of packet timings we got (including those we discarded) */
	int32_t timing[MAX_TIMINGS] = {}; /**< Sorted list of all timings ("latest" packets first) */
	int16_t counts[MAX_TIMINGS] = {}; /**< Order the packets were put in (will be used for short-term estimate) */

public:
	void set_filled(int p_filled) {
		filled = p_filled;
	}

	void set_curr_count(int p_curr_count) {
		curr_count = p_curr_count;
	}

	void set_timing(int index, int32_t value) {
		ERR_FAIL_INDEX(index, MAX_TIMINGS);
		timing[index] = value;
	}

	void set_counts(int index, int16_t value) {
		ERR_FAIL_INDEX(index, MAX_TIMINGS);
		counts[index] = value;
	}

	int get_filled() const {
		return filled;
	}

	int get_curr_count() const {
		return curr_count;
	}

	int32_t get_timing(int index) const {
		ERR_FAIL_INDEX_V(index, MAX_TIMINGS, 0);
		return timing[index];
	}

	int16_t get_counts(int index) const {
		ERR_FAIL_INDEX_V(index, MAX_TIMINGS, 0);
		return counts[index];
	}

	TimingBuffer() {
		for (int i = 0; i < MAX_TIMINGS; ++i) {
			timing[i] = 0;
			counts[i] = 0;
		}
	}
};

/** Jitter buffer structure */
class JitterBuffer : public RefCounted {
	GDCLASS(JitterBuffer, RefCounted);

public:
	int64_t pointer_timestamp = 0;
	int64_t last_returned_timestamp = 0;
	int64_t next_stop = 0;

	int64_t buffered = 0;

	Ref<JitterBufferPacket> packets[SPEEX_JITTER_MAX_BUFFER_SIZE];
	int64_t arrival[SPEEX_JITTER_MAX_BUFFER_SIZE];

	void (*destroy)(void *) = nullptr;

	int64_t delay_step = 0;
	int64_t concealment_size = 0;
	int reset_state = 0;
	int buffer_margin = 0;
	int late_cutoff = 0;
	int interp_requested = 0;
	int auto_adjust = 0;

	TimingBuffer *_tb[MAX_BUFFERS] = {};
	TimingBuffer *timeBuffers[MAX_BUFFERS] = {};
	int window_size = 0;
	int subwindow_size = 0;
	int max_late_rate = 0;
	int latency_tradeoff = 0;
	int auto_tradeoff = 0;

	int lost_count = 0;

public:
	JitterBuffer() {
		for (int i = 0; i < MAX_BUFFERS; ++i) {
			_tb[i] = memnew(TimingBuffer);
			timeBuffers[i] = _tb[i];
		}
	}
	~JitterBuffer() {
		for (int i = 0; i < MAX_BUFFERS; ++i) {
			memdelete_notnull(_tb[i]);
			_tb[i] = nullptr;
			timeBuffers[i] = nullptr;
		}
	}
	void set_pointer_timestamp(int64_t p_pointer_timestamp) { pointer_timestamp = p_pointer_timestamp; }
	void set_last_returned_timestamp(int64_t p_last_returned_timestamp) { last_returned_timestamp = p_last_returned_timestamp; }
	void set_next_stop(int64_t p_next_stop) { next_stop = p_next_stop; }
	void set_buffered(int64_t p_buffered) { buffered = p_buffered; }
	void set_destroy(void (*p_destroy)(void *)) { destroy = p_destroy; }
	void set_delay_step(int64_t p_delay_step) { delay_step = p_delay_step; }
	void set_concealment_size(int64_t p_concealment_size) { concealment_size = p_concealment_size; }
	void set_reset_state(int p_reset_state) { reset_state = p_reset_state; }
	void set_buffer_margin(int p_buffer_margin) { buffer_margin = p_buffer_margin; }
	void set_late_cutoff(int p_late_cutoff) { late_cutoff = p_late_cutoff; }
	void set_interp_requested(int p_interp_requested) { interp_requested = p_interp_requested; }
	void set_auto_adjust(int p_auto_adjust) { auto_adjust = p_auto_adjust; }
	void set_window_size(int p_window_size) { window_size = p_window_size; }
	void set_subwindow_size(int p_subwindow_size) { subwindow_size = p_subwindow_size; }
	void set_max_late_rate(int p_max_late_rate) { max_late_rate = p_max_late_rate; }
	void set_latency_tradeoff(int p_latency_tradeoff) { latency_tradeoff = p_latency_tradeoff; }
	void set_auto_tradeoff(int p_auto_tradeoff) { auto_tradeoff = p_auto_tradeoff; }
	void set_lost_count(int p_lost_count) { lost_count = p_lost_count; }

	int64_t get_pointer_timestamp() const { return pointer_timestamp; }
	int64_t get_last_returned_timestamp() const { return last_returned_timestamp; }
	int64_t get_next_stop() const { return next_stop; }
	int64_t get_buffered() const { return buffered; }
	void (*get_destroy())(void *) { return destroy; }
	int64_t get_delay_step() const { return delay_step; }
	int64_t get_concealment_size() const { return concealment_size; }
	int get_reset_state() const { return reset_state; }
	int get_buffer_margin() const { return buffer_margin; }
	int get_late_cutoff() const { return late_cutoff; }
	int get_interp_requested() const { return interp_requested; }
	int get_auto_adjust() const { return auto_adjust; }
	int get_window_size() const { return window_size; }
	int get_subwindow_size() const { return subwindow_size; }
	int get_max_late_rate() const { return max_late_rate; }
	int get_latency_tradeoff() const { return latency_tradeoff; }
	int get_auto_tradeoff() const { return auto_tradeoff; }
	int get_lost_count() const { return lost_count; }

protected:
	static void _bind_methods();
};

class VoipJitterBuffer : public RefCounted {
	GDCLASS(VoipJitterBuffer, RefCounted);

public:
	void jitter_buffer_reset(Ref<JitterBuffer> jitter);
	int jitter_buffer_ctl(Ref<JitterBuffer> jitter, int request, int32_t *ptr);
	Ref<JitterBuffer> jitter_buffer_init(int step_size);
	void jitter_buffer_destroy(Ref<JitterBuffer> jitter);
	void jitter_buffer_put(Ref<JitterBuffer> jitter, const Ref<JitterBufferPacket> packet);
	Array jitter_buffer_get(Ref<JitterBuffer> jitter, Ref<JitterBufferPacket> packet, int32_t desired_span);
	int jitter_buffer_get_another(Ref<JitterBuffer> jitter, Ref<JitterBufferPacket> packet);
	int32_t jitter_buffer_update_delay(Ref<JitterBuffer> jitter, Ref<JitterBufferPacket> packet);
	int jitter_buffer_get_pointer_timestamp(Ref<JitterBuffer> jitter);
	void jitter_buffer_tick(Ref<JitterBuffer> jitter);
	void jitter_buffer_remaining_span(Ref<JitterBuffer> jitter, uint32_t rem);

protected:
	static void _bind_methods();

public:
	static void tb_init(TimingBuffer *buffer);

	/* Add the timing of a new packet to the TimingBuffer */
	static void tb_add(TimingBuffer *buffer, int16_t timing);

	/** Based on available data, this computes the optimal delay for the jitter buffer.
	   The optimised function is in timestamp units and is:
	   cost = delay + late_factor*[number of frames that would be late if we used that delay]
	   @param tb Array of buffers
	   @param late_factor Equivalent cost of a late frame (in timestamp units)
	 */
	static int16_t compute_opt_delay(Ref<JitterBuffer> jitter);

	/** Take the following timing into consideration for future calculations */
	static void update_timings(Ref<JitterBuffer> jitter, int32_t timing);

	/** Compensate all timings when we do an adjustment of the buffering */
	static void shift_timings(Ref<JitterBuffer> jitter, int16_t amount);

	/* Let the jitter buffer know it's the right time to adjust the buffering delay to the network conditions */
	static int _jitter_buffer_update_delay(Ref<JitterBuffer> jitter, Ref<JitterBufferPacket> packet);
};

#endif // JITTER_H
