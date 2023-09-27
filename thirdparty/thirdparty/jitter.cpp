/**************************************************************************/
/*  jitter.cpp                                                            */
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

#include "jitter.h"
#include "core/error/error_macros.h"

void VoipJitterBuffer::jitter_buffer_reset(Ref<JitterBuffer> jitter) {
	ERR_FAIL_NULL(jitter);
	int i;
	for (i = 0; i < SPEEX_JITTER_MAX_BUFFER_SIZE; i++) {
		if (jitter->packets[i].is_null()) {
			continue;
		}
		if (jitter->packets[i]->get_data().is_empty()) {
			continue;
		}
		jitter->packets[i]->get_data().clear();
	}
	/* Timestamp is actually undefined at this point */
	jitter->pointer_timestamp = 0;
	jitter->next_stop = 0;
	jitter->reset_state = 1;
	jitter->lost_count = 0;
	jitter->buffered = 0;
	jitter->auto_tradeoff = 32000;

	for (i = 0; i < MAX_BUFFERS; i++) {
		tb_init(jitter->_tb[i]);
		jitter->timeBuffers[i] = jitter->_tb[i];
	}
	print_verbose("reset");
}

int VoipJitterBuffer::jitter_buffer_ctl(Ref<JitterBuffer> jitter, int request, int32_t *ptr) {
	ERR_FAIL_NULL_V(jitter, -1);
	int count, i;
	switch (request) {
		case JITTER_BUFFER_SET_MARGIN:
			jitter->buffer_margin = *ptr;
			break;
		case JITTER_BUFFER_GET_MARGIN:
			*ptr = jitter->buffer_margin;
			break;
		case JITTER_BUFFER_GET_AVALIABLE_COUNT:
			count = 0;
			for (i = 0; i < SPEEX_JITTER_MAX_BUFFER_SIZE; i++) {
				if (jitter->packets[i].is_null()) {
					continue;
				}
				if (jitter->packets[i]->get_data().is_empty()) {
					continue;
				}
				if (LE32(jitter->pointer_timestamp, jitter->packets[i]->get_timestamp())) {
					count++;
				}
			}
			*ptr = count;
			break;
		case JITTER_BUFFER_SET_DESTROY_CALLBACK:
			jitter->destroy = (void (*)(void *))ptr;
			break;
		case JITTER_BUFFER_GET_DESTROY_CALLBACK:
			*(void (**)(void *))ptr = jitter->destroy;
			break;
		case JITTER_BUFFER_SET_DELAY_STEP:
			jitter->delay_step = *ptr;
			break;
		case JITTER_BUFFER_GET_DELAY_STEP:
			*ptr = jitter->delay_step;
			break;
		case JITTER_BUFFER_SET_CONCEALMENT_SIZE:
			jitter->concealment_size = *ptr;
			break;
		case JITTER_BUFFER_GET_CONCEALMENT_SIZE:
			*ptr = jitter->concealment_size;
			break;
		case JITTER_BUFFER_SET_MAX_LATE_RATE:
			jitter->max_late_rate = *ptr;
			jitter->window_size = 100 * TOP_DELAY / jitter->max_late_rate;
			jitter->subwindow_size = jitter->window_size / MAX_BUFFERS;
			break;
		case JITTER_BUFFER_GET_MAX_LATE_RATE:
			*ptr = jitter->max_late_rate;
			break;
		case JITTER_BUFFER_SET_LATE_COST:
			jitter->latency_tradeoff = *ptr;
			break;
		case JITTER_BUFFER_GET_LATE_COST:
			*ptr = jitter->latency_tradeoff;
			break;
		default:
			ERR_PRINT(vformat("Unknown jitter_buffer_ctl request: %d", request));
			return -1;
	}
	return 0;
}

Ref<JitterBuffer> VoipJitterBuffer::jitter_buffer_init(int step_size) {
	Ref<JitterBuffer> jitter;
	jitter.instantiate();
	int i;
	int32_t tmp;
	for (i = 0; i < SPEEX_JITTER_MAX_BUFFER_SIZE; i++) {
		if (jitter->packets[i].is_null()) {
			continue;
		}
		if (jitter->packets[i]->get_data().is_empty()) {
			continue;
		}
		jitter->packets[i]->get_data().clear();
	}
	jitter->delay_step = step_size;
	jitter->concealment_size = step_size;
	/*FIXME: Should this be 0 or 1?*/
	jitter->buffer_margin = 0;
	jitter->late_cutoff = 50;
	jitter->destroy = nullptr;
	jitter->latency_tradeoff = 0;
	jitter->auto_adjust = 1;
	tmp = 4;
	jitter_buffer_ctl(jitter, JITTER_BUFFER_SET_MAX_LATE_RATE, &tmp);
	jitter_buffer_reset(jitter);
	return jitter;
}

void VoipJitterBuffer::jitter_buffer_destroy(Ref<JitterBuffer> jitter) {
	ERR_FAIL_NULL(jitter);
	jitter_buffer_reset(jitter);
}

void VoipJitterBuffer::jitter_buffer_put(Ref<JitterBuffer> jitter, const Ref<JitterBufferPacket> packet) {
	ERR_FAIL_NULL(jitter);
	ERR_FAIL_NULL(packet);
	int i_jitter = 0, j_jitter = 0;
	int late;

	// Cleanup buffer (remove old packets that weren't played)
	if (!jitter->reset_state) {
		for (i_jitter = 0; i_jitter < SPEEX_JITTER_MAX_BUFFER_SIZE; i_jitter++) {
			if (jitter->packets[i_jitter].is_valid() && LE32(jitter->packets[i_jitter]->get_timestamp() + jitter->packets[i_jitter]->get_span(), jitter->pointer_timestamp)) {
				if (jitter->packets[i_jitter]->get_data().is_empty()) {
					continue;
				}
				jitter->packets[i_jitter]->get_data().clear();
			}
		}
	}

	// Find an empty slot for the new packet
	for (i_jitter = 0; i_jitter < SPEEX_JITTER_MAX_BUFFER_SIZE; i_jitter++) {
		if (jitter->packets[i_jitter] == nullptr || jitter->packets[i_jitter]->get_data().is_empty()) {
			break;
		}
	}

	// Check if packet is late (could still be useful though)
	if (jitter->packets[i_jitter].is_valid() && !jitter->reset_state && LT32(packet->get_timestamp(), jitter->next_stop)) {
		update_timings(jitter, ((int32_t)packet->get_timestamp()) - ((int32_t)jitter->next_stop) - jitter->buffer_margin);
		late = 1;
	} else {
		late = 0;
	}

	// For some reason, the consumer has failed the last 20 fetches. Make sure this packet is used to resync.
	if (jitter->packets[i_jitter].is_valid() && jitter->lost_count > 20) {
		jitter_buffer_reset(jitter);
	}

	/* No place left in the buffer, need to make room for it by discarding the oldest packet */
	if (i_jitter == SPEEX_JITTER_MAX_BUFFER_SIZE) {
		int64_t earliest = 0;
		i_jitter = 0;

		// Find the first non-null packet and set its timestamp as the initial value of 'earliest'
		for (; i_jitter < SPEEX_JITTER_MAX_BUFFER_SIZE; i_jitter++) {
			if (jitter->packets[i_jitter] != nullptr) {
				earliest = jitter->packets[i_jitter]->get_timestamp();
				break;
			}
		}

		for (j_jitter = 1; j_jitter < SPEEX_JITTER_MAX_BUFFER_SIZE; j_jitter++) {
			if (jitter->packets[j_jitter] == nullptr || jitter->packets[j_jitter]->get_data().is_empty()) {
				continue;
			}
			if (LT32(jitter->packets[j_jitter]->get_timestamp(), earliest)) {
				earliest = jitter->packets[j_jitter]->get_timestamp();
				i_jitter = j_jitter;
			}
		}
		if (jitter->packets[i_jitter] != nullptr) {
			jitter->packets[i_jitter]->get_data().clear();
		}

		// Shift non-null packets to the left, filling the gaps created by nulls
		for (int k_jitter = 0, l_jitter = 0; k_jitter < SPEEX_JITTER_MAX_BUFFER_SIZE; k_jitter++) {
			if (jitter->packets[k_jitter] != nullptr && !jitter->packets[k_jitter]->get_data().is_empty()) {
				if (k_jitter != l_jitter) {
					jitter->packets[l_jitter] = jitter->packets[k_jitter];
					jitter->packets[k_jitter] = Ref<JitterBufferPacket>(memnew(JitterBufferPacket));
				}
				l_jitter++;
			}
			i_jitter = l_jitter;
		}
		print_verbose(vformat("Buffer is full, discarding earliest frame %d (currently at %d)", packet->get_timestamp(), jitter->pointer_timestamp));		
	}

	// Check if the packet object is valid before copying data and setting properties
	if (jitter->packets[i_jitter].is_null()) {
		jitter->packets[i_jitter].instantiate();
	}

	/* Copy packet in buffer */
	jitter->packets[i_jitter]->get_data() = packet->get_data();

	jitter->packets[i_jitter]->set_timestamp(packet->get_timestamp());
	jitter->packets[i_jitter]->set_span(packet->get_span());
	jitter->packets[i_jitter]->set_sequence(packet->get_sequence());
	jitter->packets[i_jitter]->set_user_data(packet->get_user_data());
	if (jitter->reset_state || late) {
		jitter->arrival[i_jitter] = 0;
	} else {
		jitter->arrival[i_jitter] = jitter->next_stop;
	}
}

Array VoipJitterBuffer::jitter_buffer_get(Ref<JitterBuffer> jitter, Ref<JitterBufferPacket> packet, int32_t desired_span) {
	Array array;
	array.resize(2);
	array[0] = JITTER_BUFFER_MISSING;
	int32_t start_offset = 0;
	array[1] = start_offset;
	ERR_FAIL_NULL_V(jitter, array);
	ERR_FAIL_NULL_V(packet, array);
	int i = 0;
	int16_t opt;

	/* Syncing on the first call */
	if (jitter->reset_state) {
		int found = 0;
		/* Find the oldest packet */
		uint32_t oldest = 0;
		for (i = 0; i < SPEEX_JITTER_MAX_BUFFER_SIZE; i++) {
			if (jitter->packets[i].is_null()) {
				continue;
			}
			if (jitter->packets[i]->get_data().is_empty()) {
				continue;
			}
			if ((!found || LT32(jitter->packets[i]->get_timestamp(), oldest))) {
				oldest = jitter->packets[i]->get_timestamp();
				found = 1;
			}
		}
		if (found) {
			jitter->reset_state = 0;
			jitter->pointer_timestamp = oldest;
			jitter->next_stop = oldest;
		} else {
			packet->set_timestamp(0);
			packet->set_span(jitter->interp_requested);
			array.resize(2);
			array[0] = JITTER_BUFFER_MISSING;
			array[1] = start_offset;
			return array;
		}
	}

	jitter->last_returned_timestamp = jitter->pointer_timestamp;

	if (jitter->interp_requested != 0) {
		if (packet.is_null()) {
			packet.instantiate();
		}
		packet->set_timestamp(jitter->pointer_timestamp);
		packet->set_span(jitter->interp_requested);

		/* Increment the pointer because it got decremented in the delay update */
		jitter->pointer_timestamp += jitter->interp_requested;
		packet->get_data().clear();
		print_verbose("Deferred interpolate");

		jitter->interp_requested = 0;

		jitter->buffered = packet->get_span() - desired_span;
		array.resize(2);
		array[0] = JITTER_BUFFER_INSERTION;
		array[1] = start_offset;
		return array;
	}

	/* Searching for the packet that fits best */

	/* Search the buffer for a packet with the right timestamp and spanning the whole current chunk */
	for (i = 0; i < SPEEX_JITTER_MAX_BUFFER_SIZE; i++) {
		if (jitter->packets[i].is_null()) {
			continue;
		}
		if (jitter->packets[i]->get_data().is_empty()) {
			continue;
		}
		if (jitter->packets[i]->get_timestamp() == jitter->pointer_timestamp && GE32(jitter->packets[i]->get_timestamp() + jitter->packets[i]->get_span(), jitter->pointer_timestamp + desired_span)) {
			break;
		}
	}

	/* If no match, try for an "older" packet that still spans (fully) the current chunk */
	if (i == SPEEX_JITTER_MAX_BUFFER_SIZE) {
		for (i = 0; i < SPEEX_JITTER_MAX_BUFFER_SIZE; i++) {
			if (jitter->packets[i].is_null()) {
				continue;
			}
			if (jitter->packets[i]->get_data().is_empty()) {
				continue;
			}
			if (LE32(jitter->packets[i]->get_timestamp(), jitter->pointer_timestamp) && GE32(jitter->packets[i]->get_timestamp() + jitter->packets[i]->get_span(), jitter->pointer_timestamp + desired_span)) {
				break;
			}
		}
	}

	/* If still no match, try for an "older" packet that spans part of the current chunk */
	if (i == SPEEX_JITTER_MAX_BUFFER_SIZE) {
		for (i = 0; i < SPEEX_JITTER_MAX_BUFFER_SIZE; i++) {
			if (jitter->packets[i].is_null()) {
				continue;
			}
			if (jitter->packets[i]->get_data().is_empty()) {
				continue;
			}
			if (LE32(jitter->packets[i]->get_timestamp(), jitter->pointer_timestamp) && GT32(jitter->packets[i]->get_timestamp() + jitter->packets[i]->get_span(), jitter->pointer_timestamp)) {
				break;
			}
		}
	}

	/* If still no match, try for earliest packet possible */
	if (i == SPEEX_JITTER_MAX_BUFFER_SIZE) {
		int found = 0;
		uint32_t best_time = 0;
		int best_span = 0;
		int besti = 0;
		for (i = 0; i < SPEEX_JITTER_MAX_BUFFER_SIZE; i++) {
			if (jitter->packets[i].is_null()) {
				continue;
			}
			if (jitter->packets[i]->get_data().is_empty()) {
				continue;
			}
			/* check if packet starts within current chunk */
			if (LT32(jitter->packets[i]->get_timestamp(), jitter->pointer_timestamp + desired_span) && GE32(jitter->packets[i]->get_timestamp(), jitter->pointer_timestamp)) {
				if (!found || LT32(jitter->packets[i]->get_timestamp(), best_time) || (jitter->packets[i]->get_timestamp() == best_time && GT32(jitter->packets[i]->get_span(), best_span))) {
					best_time = jitter->packets[i]->get_timestamp();
					best_span = jitter->packets[i]->get_span();
					besti = i;
					found = 1;
				}
			}
		}
		if (found) {
			i = besti;
			/*fprintf (stderr, "incomplete: %d %d %d %d\n", jitter->packets[i]->timestamp, jitter->pointer_timestamp, chunk_size, jitter->packets[i]->span);*/
		}
	}

	/* If we find something */
	if (i != SPEEX_JITTER_MAX_BUFFER_SIZE) {
		int32_t offset;

		/* We (obviously) haven't lost this packet */
		jitter->lost_count = 0;

		/* In this case, 0 isn't as a valid timestamp */
		if (jitter->arrival[i] != 0) {
			update_timings(jitter, ((int32_t)jitter->packets[i]->get_timestamp()) - ((int32_t)jitter->arrival[i]) - jitter->buffer_margin);
		}

		if (packet.is_null()) {
			packet.instantiate();
		}

		/* Copy packet */
		packet->set_data(jitter->packets[i]->get_data());
		jitter->packets[i]->get_data().clear();

		/* Set timestamp and span (if requested) */
		offset = (int32_t)jitter->packets[i]->get_timestamp() - (int32_t)jitter->pointer_timestamp;
		start_offset = offset;
		if (offset != 0) {
			ERR_PRINT(vformat("jitter_buffer_get() discarding non-zero start_offset %d", offset));
		}

		packet->set_timestamp(jitter->packets[i]->get_timestamp());
		jitter->last_returned_timestamp = packet->get_timestamp();

		packet->set_span(jitter->packets[i]->get_span());
		packet->set_sequence(jitter->packets[i]->get_sequence());
		packet->set_user_data(jitter->packets[i]->get_user_data());
		/* Point to the end of the current packet */
		jitter->pointer_timestamp = jitter->packets[i]->get_timestamp() + jitter->packets[i]->get_span();

		jitter->buffered = packet->get_span() - desired_span;

		jitter->buffered += start_offset;

		array.resize(2);
		array[0] = JITTER_BUFFER_OK;
		array[1] = start_offset;
		return array;
	}

	/* If we haven't found anything worth returning */

	print_verbose("not found");
	jitter->lost_count++;
	print_verbose("m");
	print_verbose(vformat("lost_count = %d\n", jitter->lost_count));

	opt = compute_opt_delay(jitter);

	/* Should we force an increase in the buffer or just do normal interpolation? */
	if (opt < 0) {
		/* Need to increase buffering */

		/* Shift histogram to compensate */
		shift_timings(jitter, -opt);

		packet->set_timestamp(jitter->pointer_timestamp);
		packet->set_span(-opt);
		/* Don't move the pointer_timestamp forward */
		packet->get_data().clear();

		jitter->buffered = packet->get_span() - desired_span;
		array.resize(2);
		array[0] = JITTER_BUFFER_INSERTION;
		array[1] = start_offset;
		/*jitter->pointer_timestamp -= jitter->delay_step;*/
		print_verbose(vformat("Forced to interpolate."));
	} else {
		/* Normal packet loss */
		packet->set_timestamp(jitter->pointer_timestamp);
		if (jitter->concealment_size != 0) {
			desired_span = ROUND_DOWN(desired_span, jitter->concealment_size);
		}
		packet->set_span(desired_span);
		jitter->pointer_timestamp += desired_span;
		packet->get_data().clear();

		jitter->buffered = packet->get_span() - desired_span;
		array.resize(2);
		array[0] = JITTER_BUFFER_MISSING;
		array[1] = start_offset;
	}
	return array;
}

int VoipJitterBuffer::jitter_buffer_get_another(Ref<JitterBuffer> jitter, Ref<JitterBufferPacket> packet) {
	ERR_FAIL_NULL_V(jitter, JITTER_BUFFER_MISSING);
	ERR_FAIL_NULL_V(packet, JITTER_BUFFER_MISSING);
	int i;
	for (i = 0; i < SPEEX_JITTER_MAX_BUFFER_SIZE; i++) {
		if (jitter->packets[i].is_null()) {
			continue;
		}
		if (jitter->packets[i]->get_data().is_empty()) {
			continue;
		}
		if (jitter->packets[i]->get_timestamp() == jitter->last_returned_timestamp) {
			break;
		}
	}
	if (i != SPEEX_JITTER_MAX_BUFFER_SIZE) {
		/* Copy packet */
		if (jitter->packets[i].is_null()) {
			packet->set_data(PackedByteArray());
		} else {
			packet->set_data(jitter->packets[i]->get_data());
			jitter->packets[i]->get_data().clear();
		}
		packet->set_timestamp(jitter->packets[i]->get_timestamp());
		packet->set_span(jitter->packets[i]->get_span());
		packet->set_sequence(jitter->packets[i]->get_sequence());
		packet->set_user_data(jitter->packets[i]->get_user_data());
		return JITTER_BUFFER_OK;
	} else {
		if (packet.is_valid()) {
			packet->get_data().clear();
		}
		packet->set_span(0);
		return JITTER_BUFFER_MISSING;
	}
}

int32_t VoipJitterBuffer::jitter_buffer_update_delay(Ref<JitterBuffer> jitter, Ref<JitterBufferPacket> packet) {
	ERR_FAIL_NULL_V(jitter, 0);
	ERR_FAIL_NULL_V(packet, 0);
	/* If the programmer calls jitter_buffer_update_delay() directly,
	   automatically disable auto-adjustment */
	jitter->auto_adjust = 0;
	return _jitter_buffer_update_delay(jitter, packet);
}

int VoipJitterBuffer::jitter_buffer_get_pointer_timestamp(Ref<JitterBuffer> jitter) {
	ERR_FAIL_NULL_V(jitter, 0);
	return jitter->pointer_timestamp;
}

void VoipJitterBuffer::jitter_buffer_tick(Ref<JitterBuffer> jitter) {
	ERR_FAIL_NULL(jitter);
	/* Automatically-adjust the buffering delay if requested */
	if (jitter->auto_adjust) {
		_jitter_buffer_update_delay(jitter, nullptr);
	}

	if (jitter->buffered >= 0) {
		jitter->next_stop = jitter->pointer_timestamp - jitter->buffered;
	} else {
		jitter->next_stop = jitter->pointer_timestamp;
		ERR_PRINT(vformat("jitter buffer sees negative buffering, your code might be broken. Value is %d", jitter->buffered));
	}
	jitter->buffered = 0;
}

void VoipJitterBuffer::jitter_buffer_remaining_span(Ref<JitterBuffer> jitter, uint32_t rem) {
	ERR_FAIL_NULL(jitter);
	/* Automatically-adjust the buffering delay if requested */
	if (jitter->auto_adjust) {
		_jitter_buffer_update_delay(jitter, nullptr);
	}

	if (jitter->buffered < 0) {
		ERR_PRINT(vformat("jitter buffer sees negative buffering, your code might be broken. Value is %d", jitter->buffered));
	}
	jitter->next_stop = jitter->pointer_timestamp - rem;
}

void VoipJitterBuffer::tb_init(TimingBuffer *tb) {
	ERR_FAIL_NULL(tb);
	tb->set_filled(0);
	tb->set_curr_count(0);
}

void VoipJitterBuffer::tb_add(TimingBuffer *tb, int16_t timing) {
	ERR_FAIL_NULL(tb);
	int pos;
	/* Discard packet that won't make it into the list because they're too early */
	if (tb->get_filled() >= MAX_TIMINGS && timing >= tb->get_timing(tb->get_filled() - 1)) {
		tb->set_curr_count(tb->get_curr_count() + 1);
		return;
	}

	/* Find where the timing info goes in the sorted list using binary search */
	int32_t left = 0, right = tb->get_filled() - 1;
	while (left <= right) {
		int32_t mid = left + (right - left) / 2;
		if (tb->get_timing(mid) < timing) { // Checked the original code, it's correct.
			left = mid + 1;
		} else {
			right = mid - 1;
		}
	}
	pos = left;

	ERR_FAIL_COND(!(pos <= tb->get_filled() && pos < MAX_TIMINGS));

	/* Shift everything so we can perform the insertion */
	if (pos < tb->get_filled()) {
		int move_size = tb->get_filled() - pos;
		if (tb->get_filled() == MAX_TIMINGS) {
			move_size -= 1;
		}
		for (int i = 0; i < move_size; ++i) {
			tb->set_timing(pos + 1 + i, tb->get_timing(pos + i));
			tb->set_counts(pos + 1 + i, tb->get_counts(pos + i));
		}
	}
	/* Insert */
	tb->set_timing(pos, timing);
	tb->set_counts(pos, tb->get_curr_count());

	tb->set_curr_count(tb->get_curr_count() + 1);
	if (tb->get_filled() < MAX_TIMINGS) {
		tb->set_filled(tb->get_filled() + 1);
	}
}

int16_t VoipJitterBuffer::compute_opt_delay(Ref<JitterBuffer> jitter) {
	ERR_FAIL_NULL_V(jitter, 0);
	int i;
	int16_t opt = 0;
	int32_t best_cost = 0x7fffffff;
	int late = 0;
	int pos[MAX_BUFFERS];
	int tot_count;
	float late_factor;
	int penalty_taken = 0;
	int best = 0;
	int worst = 0;
	int32_t deltaT;

	/* Number of packet timings we have received (including those we didn't keep) */
	tot_count = 0;
	for (i = 0; i < MAX_BUFFERS; i++) {
		tot_count += jitter->_tb[i]->get_curr_count();
	}
	if (tot_count == 0) {
		return 0;
	}

	/* Compute cost for one lost packet */
	if (jitter->latency_tradeoff != 0) {
		late_factor = jitter->latency_tradeoff * 100.0f / tot_count;
	} else {
		late_factor = jitter->auto_tradeoff * jitter->window_size / tot_count;
	}

	print_verbose(vformat("late_factor = %f\n", late_factor));
	for (i = 0; i < MAX_BUFFERS; i++) {
		pos[i] = 0;
	}

	/* Pick the TOP_DELAY "latest" packets (doesn't need to actually be late
	   for the current settings) */
	for (i = 0; i < TOP_DELAY; i++) {
		int j;
		int next = -1;
		int latest = 32767;
		/* Pick latest among all sub-windows */
		for (j = 0; j < MAX_BUFFERS; j++) {
			if (pos[j] < jitter->_tb[j]->get_filled() && jitter->_tb[j]->get_timing(pos[j]) < latest) {
				next = j;
				latest = jitter->_tb[j]->get_timing(pos[j]);
			}
		}
		if (next != -1) {
			int32_t cost;

			if (i == 0) {
				worst = latest;
			}
			best = latest;
			latest = ROUND_DOWN(latest, jitter->delay_step);
			pos[next]++;

			/* Actual cost function that tells us how bad using this delay would be */
			cost = -latest + late_factor * late;
			/*fprintf(stderr, "cost %d = %d + %f * %d\n", cost, -latest, late_factor, late);*/
			if (cost < best_cost) {
				best_cost = cost;
				opt = latest;
			}
		} else {
			break;
		}

		/* For the next timing we will consider, there will be one more late packet to count */
		late++;
		/* Two-frame penalty if we're going to increase the amount of late frames (hysteresis) */
		if (latest >= 0 && !penalty_taken) {
			penalty_taken = 1;
			late += 4;
		}
	}

	deltaT = best - worst;
	/* This is a default "automatic latency tradeoff" when none is provided */
	jitter->auto_tradeoff = 1 + deltaT / TOP_DELAY;
	print_verbose(vformat("auto_tradeoff = %d (%d %d %d)\n", jitter->auto_tradeoff, best, worst, i));

	/* FIXME: Compute a short-term estimate too and combine with the long-term one */

	/* Prevents reducing the buffer size when we haven't really had much data */
	if (tot_count < TOP_DELAY && opt > 0) {
		return 0;
	}
	return opt;
}

void VoipJitterBuffer::update_timings(Ref<JitterBuffer> jitter, int32_t timing) {
	ERR_FAIL_NULL(jitter);
	if (timing < -32767) {
		timing = -32767;
	}
	if (timing > 32767) {
		timing = 32767;
	}
	/* If the current sub-window is full, perform a rotation and discard oldest sub-window */
	if (jitter->timeBuffers[0]->get_curr_count() >= jitter->subwindow_size) {
		int i;
		print_verbose("Rotate buffer");
		TimingBuffer *tmp = jitter->timeBuffers[MAX_BUFFERS - 1];
		for (i = MAX_BUFFERS - 1; i >= 1; i--) {
			jitter->timeBuffers[i] = jitter->timeBuffers[i - 1];
		}
		jitter->timeBuffers[0] = tmp;
		tb_init(jitter->timeBuffers[0]);
	}
	tb_add(jitter->timeBuffers[0], timing);
}

void VoipJitterBuffer::shift_timings(Ref<JitterBuffer> jitter, int16_t amount) {
	ERR_FAIL_NULL(jitter);
	int i, j;
	for (i = 0; i < MAX_BUFFERS; i++) {
		for (j = 0; j < jitter->timeBuffers[i]->get_filled(); j++) {
			jitter->timeBuffers[i]->set_timing(j, jitter->timeBuffers[i]->get_timing(j) + amount);
		}
	}
}

int32_t VoipJitterBuffer::_jitter_buffer_update_delay(Ref<JitterBuffer> jitter, Ref<JitterBufferPacket> packet) {
	ERR_FAIL_NULL_V(jitter, 0);
	ERR_FAIL_NULL_V(packet, 0);
	int16_t opt = compute_opt_delay(jitter);
	print_verbose(vformat("opt adjustment is %d ", opt));
	if (opt < 0) {
		shift_timings(jitter, -opt);
		jitter->pointer_timestamp += opt;
		jitter->interp_requested = -opt;
		print_verbose(vformat("Decision to interpolate %d samples\n", -opt));
	} else if (opt > 0) {
		shift_timings(jitter, -opt);
		jitter->pointer_timestamp += opt;
		print_verbose(vformat("Decision to drop %d samples\n", opt));
	}
	return opt;
}

void JitterBufferPacket::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_data", "data"), &JitterBufferPacket::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &JitterBufferPacket::get_data);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data"), "set_data", "get_data");

	ClassDB::bind_method(D_METHOD("set_timestamp", "timestamp"), &JitterBufferPacket::set_timestamp);
	ClassDB::bind_method(D_METHOD("get_timestamp"), &JitterBufferPacket::get_timestamp);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "timestamp"), "set_timestamp", "get_timestamp");

	ClassDB::bind_method(D_METHOD("set_span", "span"), &JitterBufferPacket::set_span);
	ClassDB::bind_method(D_METHOD("get_span"), &JitterBufferPacket::get_span);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "span"), "set_span", "get_span");

	ClassDB::bind_method(D_METHOD("set_sequence", "sequence"), &JitterBufferPacket::set_sequence);
	ClassDB::bind_method(D_METHOD("get_sequence"), &JitterBufferPacket::get_sequence);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sequence"), "set_sequence", "get_sequence");

	ClassDB::bind_method(D_METHOD("set_user_data", "user_data"), &JitterBufferPacket::set_user_data);
	ClassDB::bind_method(D_METHOD("get_user_data"), &JitterBufferPacket::get_user_data);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "user_data"), "set_user_data", "get_user_data");
}

void JitterBufferPacket::set_data(const PackedByteArray &p_data) {
	data = p_data;
}

void JitterBufferPacket::set_timestamp(int64_t p_timestamp) {
	timestamp_usec = p_timestamp;
}

void JitterBufferPacket::set_span(int64_t p_span) {
	span = p_span;
}

void JitterBufferPacket::set_sequence(int64_t p_sequence) {
	sequence = p_sequence;
}

void JitterBufferPacket::set_user_data(int64_t p_user_data) {
	user_data = p_user_data;
}

PackedByteArray JitterBufferPacket::get_data() const {
	return data;
}

int64_t JitterBufferPacket::get_timestamp() const {
	return timestamp_usec;
}

int64_t JitterBufferPacket::get_span() const {
	return span;
}

int64_t JitterBufferPacket::get_sequence() const {
	return sequence;
}

int64_t JitterBufferPacket::get_user_data() const {
	return user_data;
}

void VoipJitterBuffer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("jitter_buffer_reset", "jitter"), &VoipJitterBuffer::jitter_buffer_reset);
	// ClassDB::bind_method(D_METHOD("jitter_buffer_ctl", "jitter", "request", "value"), &VoipJitterBuffer::jitter_buffer_ctl);
	ClassDB::bind_method(D_METHOD("jitter_buffer_init", "step_size"), &VoipJitterBuffer::jitter_buffer_init);
	ClassDB::bind_method(D_METHOD("jitter_buffer_destroy", "jitter"), &VoipJitterBuffer::jitter_buffer_destroy);
	ClassDB::bind_method(D_METHOD("jitter_buffer_put", "jitter", "packet"), &VoipJitterBuffer::jitter_buffer_put);
	ClassDB::bind_method(D_METHOD("jitter_buffer_get", "jitter", "packet", "desired_span"), &VoipJitterBuffer::jitter_buffer_get);
	ClassDB::bind_method(D_METHOD("jitter_buffer_get_another", "jitter", "packet"), &VoipJitterBuffer::jitter_buffer_get_another);
	ClassDB::bind_method(D_METHOD("jitter_buffer_update_delay", "jitter", "packet"), &VoipJitterBuffer::jitter_buffer_update_delay);
	ClassDB::bind_method(D_METHOD("jitter_buffer_get_pointer_timestamp", "jitter"), &VoipJitterBuffer::jitter_buffer_get_pointer_timestamp);
	ClassDB::bind_method(D_METHOD("jitter_buffer_tick", "jitter"), &VoipJitterBuffer::jitter_buffer_tick);
	ClassDB::bind_method(D_METHOD("jitter_buffer_remaining_span", "jitter", "rem"), &VoipJitterBuffer::jitter_buffer_remaining_span);
}

void JitterBuffer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pointer_timestamp", "p_pointer_timestamp"), &JitterBuffer::set_pointer_timestamp);
	ClassDB::bind_method(D_METHOD("set_last_returned_timestamp", "p_last_returned_timestamp"), &JitterBuffer::set_last_returned_timestamp);
	ClassDB::bind_method(D_METHOD("set_next_stop", "p_next_stop"), &JitterBuffer::set_next_stop);
	ClassDB::bind_method(D_METHOD("set_buffered", "p_buffered"), &JitterBuffer::set_buffered);
	ClassDB::bind_method(D_METHOD("set_delay_step", "p_delay_step"), &JitterBuffer::set_delay_step);
	ClassDB::bind_method(D_METHOD("set_concealment_size", "p_concealment_size"), &JitterBuffer::set_concealment_size);
	ClassDB::bind_method(D_METHOD("set_reset_state", "p_reset_state"), &JitterBuffer::set_reset_state);
	ClassDB::bind_method(D_METHOD("set_buffer_margin", "p_buffer_margin"), &JitterBuffer::set_buffer_margin);
	ClassDB::bind_method(D_METHOD("set_late_cutoff", "p_late_cutoff"), &JitterBuffer::set_late_cutoff);
	ClassDB::bind_method(D_METHOD("set_interp_requested", "p_interp_requested"), &JitterBuffer::set_interp_requested);
	ClassDB::bind_method(D_METHOD("set_auto_adjust", "p_auto_adjust"), &JitterBuffer::set_auto_adjust);
	ClassDB::bind_method(D_METHOD("set_window_size", "p_window_size"), &JitterBuffer::set_window_size);
	ClassDB::bind_method(D_METHOD("set_subwindow_size", "p_subwindow_size"), &JitterBuffer::set_subwindow_size);
	ClassDB::bind_method(D_METHOD("set_max_late_rate", "p_max_late_rate"), &JitterBuffer::set_max_late_rate);
	ClassDB::bind_method(D_METHOD("set_latency_tradeoff", "p_latency_tradeoff"), &JitterBuffer::set_latency_tradeoff);
	ClassDB::bind_method(D_METHOD("set_auto_tradeoff", "p_auto_tradeoff"), &JitterBuffer::set_auto_tradeoff);
	ClassDB::bind_method(D_METHOD("set_lost_count", "p_lost_count"), &JitterBuffer::set_lost_count);

	ClassDB::bind_method(D_METHOD("get_pointer_timestamp"), &JitterBuffer::get_pointer_timestamp);
	ClassDB::bind_method(D_METHOD("get_last_returned_timestamp"), &JitterBuffer::get_last_returned_timestamp);
	ClassDB::bind_method(D_METHOD("get_next_stop"), &JitterBuffer::get_next_stop);
	ClassDB::bind_method(D_METHOD("get_buffered"), &JitterBuffer::get_buffered);
	ClassDB::bind_method(D_METHOD("get_delay_step"), &JitterBuffer::get_delay_step);
	ClassDB::bind_method(D_METHOD("get_concealment_size"), &JitterBuffer::get_concealment_size);
	ClassDB::bind_method(D_METHOD("get_reset_state"), &JitterBuffer::get_reset_state);
	ClassDB::bind_method(D_METHOD("get_buffer_margin"), &JitterBuffer::get_buffer_margin);
	ClassDB::bind_method(D_METHOD("get_late_cutoff"), &JitterBuffer::get_late_cutoff);
	ClassDB::bind_method(D_METHOD("get_interp_requested"), &JitterBuffer::get_interp_requested);
	ClassDB::bind_method(D_METHOD("get_auto_adjust"), &JitterBuffer::get_auto_adjust);
	ClassDB::bind_method(D_METHOD("get_window_size"), &JitterBuffer::get_window_size);
	ClassDB::bind_method(D_METHOD("get_subwindow_size"), &JitterBuffer::get_subwindow_size);
	ClassDB::bind_method(D_METHOD("get_max_late_rate"), &JitterBuffer::get_max_late_rate);
	ClassDB::bind_method(D_METHOD("get_latency_tradeoff"), &JitterBuffer::get_latency_tradeoff);
	ClassDB::bind_method(D_METHOD("get_auto_tradeoff"), &JitterBuffer::get_auto_tradeoff);
	ClassDB::bind_method(D_METHOD("get_lost_count"), &JitterBuffer::get_lost_count);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "pointer_timestamp"), "set_pointer_timestamp", "get_pointer_timestamp");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "last_returned_timestamp"), "set_last_returned_timestamp", "get_last_returned_timestamp");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "next_stop"), "set_next_stop", "get_next_stop");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "buffered"), "set_buffered", "get_buffered");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "delay_step"), "set_delay_step", "get_delay_step");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "concealment_size"), "set_concealment_size", "get_concealment_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reset_state"), "set_reset_state", "get_reset_state");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "buffer_margin"), "set_buffer_margin", "get_buffer_margin");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "late_cutoff"), "set_late_cutoff", "get_late_cutoff");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "interpolation_requested"), "set_interp_requested", "get_interp_requested");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "auto_adjust"), "set_auto_adjust", "get_auto_adjust");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "window_size"), "set_window_size", "get_window_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subwindow_size"), "set_subwindow_size", "get_subwindow_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_late_rate"), "set_max_late_rate", "get_max_late_rate");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "latency_tradeoff"), "set_latency_tradeoff", "get_latency_tradeoff");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "auto_tradeoff"), "set_auto_tradeoff", "get_auto_tradeoff");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lost_count"), "set_lost_count", "get_lost_count");
}
