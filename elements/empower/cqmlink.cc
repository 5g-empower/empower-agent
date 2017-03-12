/*
 * Copyright 2017 RISE SICS AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <click/config.h>
#include <elements/wifi/bitrate.hh>
#include "cqmlink.hh"
#include "empowercqm.hh"
CLICK_DECLS

CqmLink::CqmLink() {

	sourceAddr = EtherAddress();

	cqm = 0;

	iface_id = -1;

	rssiCdf = 0;
	numFramesCount = 0;
	pdr = 0;

	rssiCdf_0 = 0;
	numFramesCount_0 = 0;
	pdr_0 = 0;

	rssiCdfCount_l = 0;
	numFramesCount_l = 0;
	pdr_l = 0;

	silentWindowCount = 0;
	lastSeqNum = 0;
	currentSeqNum = 0;

	xi = 0;

    channel_busy_time = 0;
    data_bits_recv = 0;
    channel_busy_fraction = 0;
    throughput = 0;
    available_bw = 0;

    rssi_threshold = -70;
    rssi_tolerance = 0.2;
    cbt_threshold = 0.9;
    cbt_tolerance = 0.2;
    throughput_threshold = 10; // Mbps
    throughput_tolerance = 0.2;
    pdr_threshold = 0.9;
    pdr_tolerance = 0.2;

    p_pdr = 0;
    p_channel_busy_fraction = 0; // risk of channel busy fraction exceeding set threshold.
    p_throughput = 0;
    p_available_bw= 0;

    window_count = 0;

}

CqmLink::~CqmLink() {
}

// short window estimation
void CqmLink::estimator(unsigned window_period, bool debug) {

    uint16_t num_estimates = 20;
    double raw_mcs_rate = 54; // in Mbps
    double payload_efficiency = 0.6; // fraction of time spent sending payload in reference to the total time spent
    uint32_t us_window_period = window_period * 1000; // us

	if (window_count < num_estimates) {

		// Bayesian analysis: Update previous window posterior as prior for this window
		rssiCdf_0 = rssiCdf;
		numFramesCount_0 = numFramesCount;
		pdr_0 = pdr;

		// this value if for when rts cts is disabled, the sending rate is 54Mbps, basic rate is 6Mbps, and data payload length is 1000B or 8000b
		channel_busy_fraction = channel_busy_time / (double) us_window_period; //channel busy time is in micro s
		throughput = (double) data_bits_recv / (double) us_window_period; // is in Mbps since window is in us units
		available_bw = raw_mcs_rate * ( cbt_threshold - channel_busy_fraction) * payload_efficiency;

		// Bayesian analysis: Compute Posterior
		if (numFramesCount_l > 0) {

			// at least one frame received in window
			rssiCdf = (double) (rssiCdf_0 * (double) numFramesCount_0
					+ (double) rssiCdfCount_l)
					/ (double) (numFramesCount_0 + numFramesCount_l);

			if ((currentSeqNum - lastSeqNum) < 0) {
				pdr_l = (double) (numFramesCount_l)
						/ (double) (4095 - lastSeqNum + currentSeqNum + 1);
			} else {
				pdr_l = (double) (numFramesCount_l)
						/ (double) (currentSeqNum - lastSeqNum);
			}

			pdr = (pdr_0 * (double) numFramesCount_0
					+ pdr_l * (double) numFramesCount_l)
					/ (double) (numFramesCount_0 + numFramesCount_l);

			numFramesCount = numFramesCount_l;
			lastSeqNum = currentSeqNum;
			silentWindowCount = 0;

		} else {

			// no frames received in window. (Silent window)
			// Current estimates remain the same as the prior or the previous window's estimates
			rssiCdf = rssiCdf_0;
			numFramesCount = numFramesCount_0;
			pdr = pdr_0;
			silentWindowCount++;

		}

		if (debug) {
			click_chatter("p_rssi:%f pdr:%f cbf:%f Th:%f avBW:%f", rssiCdf, pdr, channel_busy_fraction, throughput, available_bw);
		}

		//sics
		channel_busy_time = 0;
		data_bits_recv = 0;

		//sics
		p_channel_busy_fraction += (channel_busy_fraction > cbt_threshold ? 1 : 0); // risk of channel busy fraction exceeding set threshold.
		p_throughput += (throughput < throughput_threshold ? 1 : 0);
		p_available_bw += (available_bw < throughput_threshold ? 1 : 0);
		p_pdr += (pdr > pdr_threshold ? 1 : 0);

		numFramesCount_l = 0;
		rssiCdfCount_l = 0;
		xi = rssiCdf * pdr;
		lastEstimateTime = currentTime;

	} else {

		// long window estimation to evaluate performance degradation
		window_count = 0;
		p_channel_busy_fraction = (double) p_channel_busy_fraction / (double) num_estimates;
		p_throughput = (double) p_throughput / (double) num_estimates;
		p_available_bw = (double) p_available_bw / (double) num_estimates;
		p_pdr = (double) p_pdr / (double) num_estimates;

		if (debug) {
			click_chatter("p_pdr:%f p_cbf:%f p_Th:%f p_avBW:%f", p_pdr, p_channel_busy_fraction, p_throughput, p_available_bw);
		}

		if (p_channel_busy_fraction > cbt_tolerance) {
			// This channel is fully occupied. Admission control should not accept any more connections
			// Trigger handover of a a few clients to another AP
		}
		if (p_throughput > throughput_tolerance) {
			// The throughput on this link has exceeded the set threshold by tolerance fraction.
			//  Trigger hand over to a better link to an AP.
		}

		// The risk probabilities must be set to zero before evaluating the next window
		p_channel_busy_fraction = 0;
		p_throughput = 0;
		p_available_bw = 0;
		p_pdr = 0;

	}

	window_count += 1;

}

void CqmLink::add_sample(uint32_t len, uint8_t rssi, uint16_t seq) {
	data_bits_recv += 8 * len;
	numFramesCount_l++;
	rssiCdfCount_l = rssiCdfCount_l + (rssi > rssi_threshold ? 1 : 0);
	currentSeqNum = seq;
	currentTime.assign_now();
}

void CqmLink::add_cbt_sample(uint32_t usec) {
	channel_busy_time += usec;
}

String CqmLink::unparse() {
	StringAccum sa;
	sa << sourceAddr.unparse();
	sa << " iface_id " << iface_id;
	sa << " last_estimated " << lastEstimateTime;
	sa << " silent_window " << silentWindowCount;
	sa << "\n";
	return sa.take_string();
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(CqmLink)
