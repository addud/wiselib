/***************************************************************************
 ** This file is part of the generic algorithm library Wiselib.           **
 ** Copyright (C) 2008,2009 by the Wisebed (www.wisebed.eu) project.      **
 **                                                                       **
 ** The Wiselib is free software: you can redistribute it and/or modify   **
 ** it under the terms of the GNU Lesser General Public License as        **
 ** published by the Free Software Foundation, either version 3 of the    **
 ** License, or (at your option) any later version.                       **
 **                                                                       **
 ** The Wiselib is distributed in the hope that it will be useful,        **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of        **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
 ** GNU Lesser General Public License for more details.                   **
 **                                                                       **
 ** You should have received a copy of the GNU Lesser General Public      **
 ** License along with the Wiselib.                                       **
 ** If not, see <http://www.gnu.org/licenses/>.                           **
 ***************************************************************************/

/*
 * Author: Adrian Dudau <adrian@vermisoft.ro>
 */

#ifndef __CTP_FORWARDING_ENGINE_H__
#define __CTP_FORWARDING_ENGINE_H__

#include "util/base_classes/routing_base.h"
#include "util/pstl/vector_static.h"
#include "algorithms/routing/ctp/ctp_forwarding_engine_msg.h"
#include "algorithms/routing/ctp/ctp_types.h"
#include "algorithms/routing/ctp/ctp_debugging.h"

#define FE_MAX_EVENT_RECEIVERS 2

namespace wiselib {

template<typename OsModel_P, typename SendQueueValue_P, typename SendQueue_P,
		typename SentCache_P, typename RandomNumber_P, typename RoutingEngine_P,
		typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Debug_P = typename OsModel_P::Debug,
		typename Clock_P = typename OsModel_P::Clock>
class CtpForwardingEngine: public RoutingBase<OsModel_P, Radio_P> {
public:
	typedef OsModel_P OsModel;
	typedef SendQueueValue_P SendQueueValue;
	typedef SendQueue_P SendQueue;
	typedef SentCache_P SentCache;
	typedef RandomNumber_P RandomNumber;
	typedef RoutingEngine_P RoutingEngine;
	typedef Radio_P Radio;
	typedef Timer_P Timer;
	typedef Debug_P Debug;
	typedef Clock_P Clock;

	typedef typename Radio::node_id_t node_id_t;
	typedef typename Radio::size_t size_t;
	typedef typename Radio::block_data_t block_data_t;
	typedef typename Radio::message_id_t message_id_t;

	typedef typename Timer::millis_t millis_t;
	typedef typename Clock::time_t time_t;
	typedef typename RandomNumber::value_t value_t;

	typedef SendQueueValue fe_queue_entry_t;
	typedef typename SentCache::iterator SentCacheIterator;

	typedef CtpForwardingEngine<OsModel, SendQueueValue, SendQueue, SentCache,
			RandomNumber, RoutingEngine, Radio, Timer, Debug> self_type;
	typedef self_type* self_pointer_t;

	typedef delegate3<void, node_id_t, size_t, block_data_t*> radio_delegate_t;
	typedef vector_static<OsModel, radio_delegate_t, RADIO_BASE_MAX_RECEIVERS> RecvCallbackVector;
	typedef typename RecvCallbackVector::iterator RecvCallbackVectorIterator;

	typedef delegate1<void, uint8_t> event_delegate_t;
	typedef vector_static<OsModel, event_delegate_t, FE_MAX_EVENT_RECEIVERS> EventCallbackVector;
	typedef typename EventCallbackVector::iterator EventCallbackVectorIterator;

	typedef CtpForwardingEngineMsg<OsModel, Radio> DataMessage;

	// --------------------------------------------------------------------

	enum ErrorCodes {
		SUCCESS = OsModel::SUCCESS,
		ERR_UNSPEC = OsModel::ERR_UNSPEC,
		ERR_NOTIMPL = OsModel::ERR_NOTIMPL,
		ERR_BUSY = OsModel::ERR_BUSY
	};

	// --------------------------------------------------------------------

	enum SpecialNodeIds {
		BROADCAST_ADDRESS = Radio_P::BROADCAST_ADDRESS, ///< All nodes in communication range
		NULL_NODE_ID = Radio_P::NULL_NODE_ID ///< Unknown/No node id
	};

	// --------------------------------------------------------------------

	enum Restrictions {
		MSG_HEADER_SIZE = DataMessage::HEADER_SIZE, //FE header overhead
		MAX_MESSAGE_LENGTH = Radio_P::MAX_MESSAGE_LENGTH - sizeof(node_id_t), ///< Maximum message length for the radio - one address space
		RANDOM_MAX = RandomNumber::RANDOM_MAX ///< Maximum random number that can be generated,
	};

	// --------------------------------------------------------------------

	enum Events {
		RE_EVENT_ROUTE_NOT_FOUND = 0, RE_EVENT_ROUTE_FOUND = 1
	};

	// --------------------------------------------------------------------

	enum {
		FORWARD_PACKET_TIME = 4,
	};

	enum {
		SENDDONE_FAIL_OFFSET = 512,
		SENDDONE_NOACK_OFFSET = FORWARD_PACKET_TIME << 2,
		SENDDONE_OK_OFFSET = FORWARD_PACKET_TIME << 2,
		LOOPY_OFFSET = FORWARD_PACKET_TIME << 4,
		SENDDONE_FAIL_WINDOW = SENDDONE_FAIL_OFFSET - 1,
		LOOPY_WINDOW = LOOPY_OFFSET - 1,
		SENDDONE_NOACK_WINDOW = SENDDONE_NOACK_OFFSET - 1,
		SENDDONE_OK_WINDOW = SENDDONE_OK_OFFSET - 1,
		CONGESTED_WAIT_OFFSET = FORWARD_PACKET_TIME << 2,
		CONGESTED_WAIT_WINDOW = CONGESTED_WAIT_OFFSET - 1,
	};

	enum {
		MAX_RETRIES = 30,
	};

	enum {
		RETXTIMER = 1, CONGESTION_TIMER = 2, POST_SENDTASK = 3,
	};

	bool clientCongested;
	bool parentCongested;
	uint8_t congestionThreshold;
	bool running;
	bool radioOn;
	bool ackPending;
	bool sending;
	node_id_t lastParent;
	uint8_t seqno;

	fe_queue_entry_t clientEntries;
	fe_queue_entry_t* clientPtrs;

	// Needed for SendQueue Interface.
	SendQueue sendqueue;

	// Required by our implementation of timers.
	bool congestionTimerIsRunning;
	bool reTxTimerIsRunning;

//	// Needed for SentCache Interface.
	SentCache sentCache;
	int sentCacheSize;

	RoutingEngine cre;

	CtpForwardingEngine() {
	}

	// --------------------------------------------------------------------

	~CtpForwardingEngine() {
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("Re: Destroyed\n");
#endif
	}

	// --------------------------------------------------------------------

	int init(void) {

		init_variables();

		return SUCCESS;
	}

	// --------------------------------------------------------------------

	int init(Radio& radio, Timer& timer, Debug& debug, Clock& clock,
			RandomNumber& random_number, RoutingEngine& re) {
		radio_ = &radio;
		timer_ = &timer;
		debug_ = &debug;
		clock_ = &clock;
		random_number_ = &random_number;
		cre = re;

		init_variables();

		return SUCCESS;
	}

	// --------------------------------------------------------------------

	int destruct(void) {
		return disable_radio();
	}

	// --------------------------------------------------------------------

	///@name Routing Control
	///@{
	int enable_radio(void) {

		random_number().srand(
				clock().milliseconds(clock().time()) * (3 * self + 2));

		radio().template reg_recv_callback<self_type, &self_type::receive>(
				this);

		timer().template set_timer<self_type, &self_type::timer_elapsed>(15000,
				this, 0);

		return radio().enable_radio();
	}

	// --------------------------------------------------------------------

	int disable_radio(void) {
		return radio().disable_radio();
	}

	// ----------------------------------------------------------------------------------

	node_id_t id() {
		return radio_->id();
	}

	// --------------------------------------------------------------------

	///@name Radio Concept
	///@{
	/**
	 */
	int send(node_id_t destination, size_t len, block_data_t *data) {
		return command_Send_send(len, data);
	}

	template<class T, void (T::*TMethod)(node_id_t, size_t, block_data_t*)>
	int reg_recv_callback(T *obj_pnt) {
		if (recv_callbacks_.empty())
			recv_callbacks_.assign(RADIO_BASE_MAX_RECEIVERS,
					radio_delegate_t());

		for (unsigned int i = 0; i < recv_callbacks_.size(); ++i) {
			if (recv_callbacks_.at(i) == radio_delegate_t()) {
				recv_callbacks_.at(i) = radio_delegate_t::template from_method<
						T, TMethod>(obj_pnt);
				return i;
			}
		}

		return -1;
	}
	// --------------------------------------------------------------------
	int unreg_recv_callback(int idx) {
		recv_callbacks_.at(idx) = radio_delegate_t();
		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------

	template<class T, void (T::*TMethod)(uint8_t)>
	uint8_t reg_event_callback(T *obj_pnt) {

		if (event_callbacks_.empty())
			event_callbacks_.assign(RE_MAX_EVENT_RECEIVERS, event_delegate_t());

		for (EventCallbackVectorIterator it = event_callbacks_.begin();
				it != event_callbacks_.end(); it++) {
			if ((*it) == event_delegate_t()) {
				(*it) = event_delegate_t::template from_method<T, TMethod>(
						obj_pnt);
				return 0;
			}
		}

		return -1;
	}

	// ----------------------------------------------------------------------------------

	int unreg_event_callback(int idx) {
		event_callbacks_.at(idx) = event_delegate_t();
		return idx;
	}

private:

	typename Radio::self_pointer_t radio_;
	typename Timer::self_pointer_t timer_;
	typename Debug::self_pointer_t debug_;
	typename Clock::self_pointer_t clock_;
	typename RandomNumber::self_pointer_t random_number_;

	// --------------------------------------------------------------------

	enum TimeoutPeriods {
		BEACON_TIMER = 0,
		ROUTE_TIMER = 1,
		POST_UPDATEROUTETASK = 2,
		POST_SENDBEACONTASK = 3
	};

	// ----------------------------------------------------------------------------------

	static const char * timeout_period_message_[4];

	// --------------------------------------------------------------------

	// Node own ID
	node_id_t self;

	RecvCallbackVector recv_callbacks_;
	EventCallbackVector event_callbacks_;

	// ----------------------------------------------------------------------------------

	Radio& radio() {
		return *radio_;
	}

	Timer& timer() {
		return *timer_;
	}

	Debug& debug() {
		return *debug_;
	}

	Clock& clock() {
		return *clock_;
	}

	RandomNumber& random_number() {
		return *random_number_;
	}

	// ----------------------------------------------------------------------------------

	void init_variables(void) {
		self = radio().id();

		//TODO: Pass configuration values as template parameter

		// Since we don't know whether a timer is running, we put these bool values
		// to true when the timer is launched and to false when it fires.
		congestionTimerIsRunning = false;
		reTxTimerIsRunning = false;

		sentCacheSize = 4; //messages

		clientCongested = false;
		parentCongested = false;
		running = false;
		radioOn = true; // TO IMPLEMENT ------------- radioOn must be off then turned on when radio control event is triggered
		ackPending = false;
		sending = false;

		clientPtrs = &clientEntries; // we support one client only -> we don't use an array
		congestionThreshold = command_SendQueue_maxSize() >> 1;
		lastParent = NULL_NODE_ID; //last parent we sent to
		seqno = 0;

	}

	// --------------------------------------------------------------------
	void notify_receivers(node_id_t from, size_t len, block_data_t *data) {
		for (RecvCallbackVectorIterator it = recv_callbacks_.begin();
				it != recv_callbacks_.end(); ++it) {
			if (*it != radio_delegate_t())
				(*it)(from, len, data);
		}
	}

	// ----------------------------------------------------------------------------------

	void notify_listeners(uint8_t event) {
		for (EventCallbackVectorIterator it = event_callbacks_.begin();
				it != event_callbacks_.end(); ++it) {
			if (*it != event_delegate_t()) {
				(*it)(event);
			}

		}
	}

	// ----------------------------------------------------------------------------------

	void echo(const char *msg, ...) {
		va_list fmtargs;
		char buffer[1024];
		int i;
		for (i = 0; i < DEBUG_NODES_NR; i++) {
			if (id() == nodes[debug_nodes[i]]) {
				va_start(fmtargs, msg);
				vsnprintf(buffer, sizeof(buffer) - 1, msg, fmtargs);
				va_end(fmtargs);
				debug().debug("%d: FE: ", id());
				debug().debug(buffer);
				debug().debug("\n");
				break;
			}
		}
	}

	// -----------------------------------------------------------------------

	int setTimer(void *userdata, millis_t millis) {
		return timer().template set_timer<self_type, &self_type::timer_elapsed>(
				millis, this, userdata);
	}

	// ----------------------------------------------------------------------------------

	void timer_elapsed(void *userdata) {
		//TODO: make sure cast works - originally was cast to int
		int timeout = (int) (userdata);

		switch (timeout) {

		case RETXTIMER: {
			event_RetxmitTimer_fired();
			reTxTimerIsRunning = false;
			break;
		}
		case CONGESTION_TIMER: {
			event_CongestionTimer_fired();
			congestionTimerIsRunning = false;
			break;
		}
		case POST_SENDTASK: {
			sendTask();
			break;

			default:
			{
#ifdef FORWARDING_ENGINE_DEBUG
				debug().debug("%d: ", self);
				debug().debug("Re: TimerFiredCallback unexpected timeout: %d\n",
						timeout);
				return;
#endif
				break;
			}
		}
		}
	}
	// ----------------------------------------------------------------------------------

	/*
	 * Function for preparing a packet for forwarding. Performs
	 * a buffer swap from the message pool. If there are no free
	 * message in the pool, it returns the passed message and does not
	 * put it on the send queue.
	 */
	void forward(DataMessage* msg) {
		fe_queue_entry_t* qe;
		uint16_t gradient;

		// There are two memset useless in our implementation: they have been removed

		qe->msg = (block_data_t*) msg;
		qe->retries = MAX_RETRIES;

		if (command_SendQueue_enqueue(qe) == SUCCESS) {
			// Loop-detection code:
			if (cre.command_CtpInfo_getEtx(&gradient) == SUCCESS) {
				// We only check for loops if we know our own metric
				if (msg->etx() <= gradient) {
					// If our etx metric is less than or equal to the etx value
					// on the packet (etx of the previous hop node), then we believe
					// we are in a loop.
					// Trigger a route update and backoff.
					echo("Possible Loop Detection...");
					cre.command_CtpInfo_triggerImmediateRouteUpdate();
					startRetxmitTimer(LOOPY_WINDOW, LOOPY_OFFSET);
				}
			}

			if (!reTxTimerIsRunning) {
				// sendTask is only immediately posted if we don't detect a
				// loop.
				post_sendTask();
			}

			// Successful function exit point:
			return;
		} else {
			echo("There was a problem enqueuing to the send queue.");
		}
	}

	/*
	 * Received a message to forward. Check whether it is a duplicate by
	 * checking the packets currently in the queue as well as the
	 * send history cache (in case we recently forwarded this packet).
	 * The cache is important as nodes immediately forward packets
	 * but wait a period before retransmitting after an ack failure.
	 * If this node is a root, signal receive.
	 */
	void receive(node_id_t from, size_t len, block_data_t *data) {
		DataMessage* msg = reinterpret_cast<DataMessage*>(data);

		bool duplicate = false;
		fe_queue_entry_t* qe;
		uint8_t i, thl;

		// Update the THL here, since it has lived another hop, and so
		// that the root sees the correct THL.
		thl = msg->thl();
		thl++;
		msg->set_thl(thl);

		//See if we remember having seen this packet
		//We look in the sent cache ...
		if (command_SentCache_lookup(data)) {
			return;
		}
		//... and in the queue for duplicates
		if (command_SendQueue_size() > 0) {
			for (i = command_SendQueue_size(); --i;) {
				qe = command_SendQueue_element(i);
				if (command_CtpPacket_matchInstance(qe->msg, data)) {
					duplicate = true;
					break;
				}
			}
		}

		if (duplicate) {
			return;
		}

		// If I'm the root, signal receive.
		else if (cre.command_RootControl_isRoot()) {
			// sends the packet to application layer.
			notify_receivers(from, len - DataMessage::HEADER_SIZE,
					msg->payload());
			return;
		} else {
			forward(msg);
		}
	}

	void event_RetxmitTimer_fired() {
		sending = false;
		post_sendTask();
	}

	void event_CongestionTimer_fired() {
		post_sendTask();
	}

	bool command_CtpCongestion_isCongested() {
		// A simple predicate for now to determine congestion state of this node.
		bool congested =
				command_SendQueue_size() > congestionThreshold ? true : false;
		return ((congested || clientCongested) ? true : false);
	}

	void command_CtpCongestion_setClientCongested(bool congested) {
		bool wasCongested = command_CtpCongestion_isCongested();
		clientCongested = congested;
		if (!wasCongested && congested) {
			cre.command_CtpInfo_triggerImmediateRouteUpdate();
		} else if (wasCongested && !command_CtpCongestion_isCongested()) {
			cre.command_CtpInfo_triggerRouteUpdate();
		}
	}

	// A CTP packet ID is based on the origin and the THL field, to
	// implement duplicate suppression as described in TEP 123.
	bool command_CtpPacket_matchInstance(block_data_t* msg1,
			block_data_t* msg2) {

		DataMessage* pkt1 = reinterpret_cast<DataMessage*>(msg1);
		DataMessage* pkt2 = reinterpret_cast<DataMessage*>(msg2);

		return (pkt1->origin() == pkt2->origin()
				&& pkt1->seqno() == pkt2->seqno() && pkt1->thl() == pkt2->thl());
	}

	void startRetxmitTimer(uint16_t mask, uint16_t offset) {
		uint16_t r = random_number().rand(UINT_MAX);
		r &= mask;
		r += offset;
		setTimer((void*) RETXTIMER, r);
		reTxTimerIsRunning = true;
	}

	void startCongestionTimer(uint16_t mask, uint16_t offset) {
		uint16_t r = random_number().rand(UINT_MAX);
		r &= mask;
		r += offset;
		setTimer((void*) CONGESTION_TIMER, r);
		congestionTimerIsRunning = true;
	}

	/*
	 *  Schedule the sendTask function as soon as possible.
	 *  Note that it is not possible to directly call the function
	 *  because of the infinite recursion in the sendTask function.
	 */
	void post_sendTask() {
		setTimer((void*) POST_SENDTASK, 0);
	}

	// ----------------------------------------------------------------------------------

	error_t command_Send_send(size_t len, block_data_t* pkt) {
		fe_queue_entry_t* qe;

		if (!running) {
			return ERR_NOTIMPL;
		}

		if (len > MAX_MESSAGE_LENGTH) {
			return ERR_NOTIMPL;
		}

		DataMessage msg;
		msg.set_options(0);
		msg.set_pull();
		msg.set_origin(self);
		msg.set_seqno(seqno++);
		msg.set_thl(0);

		if (clientPtrs == NULL) {
			echo("Send.send - send failed as client is busy.");
			return ERR_BUSY;
		}

		qe = clientPtrs;
		qe->msg = msg;
		qe->len = len + DataMessage::HEADER_SIZE;
		qe->retries = MAX_RETRIES;

		if (command_SendQueue_enqueue(qe) == SUCCESS) {
			if (radioOn && !reTxTimerIsRunning) {
				post_sendTask();
			}
			clientPtrs = NULL;
			return SUCCESS;
		} else {
			echo("Send.send - Send failed as packet could not be enqueued.");
			return ERR_UNSPEC;
		}
	}

	// ----------------------------------------------------------------------------------

	void sendTask() {
		if (sending) { // NOT CHECKED
			return;
		} else if (command_SendQueue_empty()) { // CHECK -> OK: when queue empty do nothing.
			return;
		} else if (!cre.command_RootControl_isRoot()
				&& !cre.command_Routing_hasRoute()) { // CHECK -> OK: retx called after 9.76 sec.
			setTimer((void*) RETXTIMER, 10000);

			return;
		} else {
			error_t subsendResult;
			fe_queue_entry_t* qe = command_SendQueue_head();
			node_id_t dest = cre.command_Routing_nextHop();
			ctp_msg_etx_t gradient;

			if (cre.command_CtpInfo_isNeighborCongested(dest)) { // NOT CHECKED
				// Our parent is congested. We should wait.
				// Don't repost the task, CongestionTimer will do the job
				if (!parentCongested) {
					parentCongested = true;
				}
				if (!congestionTimerIsRunning) {
					startCongestionTimer(CONGESTED_WAIT_WINDOW,
							CONGESTED_WAIT_OFFSET);
				}
				return;
			}
			if (parentCongested) {
				parentCongested = false;
			}
			// Once we are here, we have decided to send the packet.
			if (command_SentCache_lookup(qe->msg)) { // NOT CHECKED
				command_SendQueue_dequeue();

				post_sendTask();
				return;
			}
			/* If our current parent is not the same as the last parent
			 we sent do, then reset the count of unacked packets: don't
			 penalize a new parent for the failures of a prior one.*/
			if (dest != lastParent) { // CHECK -> OK: qe retries initialized
				qe->retries = MAX_RETRIES;
				lastParent = dest;
			}

			if (cre.command_RootControl_isRoot()) { // CHECK -> OK: loppbacked message to app layer.
				// there is a collectionid (needed for signal receive) that is useless in our implementation: it has been removed.
				// here there is a memcpy for loopbackMsgPtr that we don't need to implement, we use msg->dup() instead later.
				ackPending = false;

				echo("sendTask - I'm root, so loopback and signal receive.");

				// TODO: Forward the packet to the application layer
//				cPacket* dupMsg = qe->msg->dup();
//				toApplicationLayer(dupMsg->decapsulate()); // dup because it's a loopback message, it must be deleted in event_SubSend_sendDone
//
//				event_SubSend_sendDone(cc2420control, SUCCESS);
				return;
			}

			// Loop-detection functionality:
			if (cre.command_CtpInfo_getEtx(&gradient) != SUCCESS) { // NOT CHECKED
				// If we have no metric, set our gradient conservatively so
				// that other nodes don't automatically drop our packets.
				gradient = 0;
			}

			(reinterpret_cast<DataMessage*>(qe->msg))->set_etx(gradient);

			ackPending = true; // the acknowledgement is automatically requested for Unicast packets in the implemented CC2420Mac

			// Set or clear the congestion bit on *outgoing* packets.
			if (command_CtpCongestion_isCongested()) {
				(reinterpret_cast<DataMessage*>(qe->msg))->set_congestion();
			} else {
				(reinterpret_cast<DataMessage*>(qe->msg))->clear_congestion();
			}

			//TODO: Send message to the RE
			subsendResult = radio().send(dest, qe->len, qe->msg);

			//TODO: Implement callback to send_done event from RE
			event_SubSend_sendDone(qe->msg, subsendResult);

//				subsendResult = db->command_Send_send(dest, qe->msg->dup()); // the duplicate will be sent via the dual buffer module. we keep a copy here that will be deleted fater the sendDone.
//				if (subsendResult == SUCCESS) { // CHECK -> OK
//					// Successfully submitted to the data-link layer.
//					return;
//				} else if (subsendResult == EOFF) { // NOT CHECKED
//					// The radio has been turned off underneath us. Assume that
//					// this is for the best. When the radio is turned back on, we'll
//					// handle a startDone event and resume sending.
//					radioOn = false;
//					emit(registerSignal("CfeTxDroppedRadioOff"), self);
//					trace() << "sendTask - Subsend failed from EOFF.";
//				} else if (subsendResult == EBUSY) { // CHECKED -> OK
//					// This shouldn't happen, as we sit on top of a client and
//					// control our own output; it means we're trying to
//					// double-send (bug). This means we expect a sendDone, so just
//					// wait for that: when the sendDone comes in, // we'll try
//					// sending this packet again.
//					emit(registerSignal("CfeTxDroppedRadioEbusy"), self);
//					trace() << "sendTask - Subsend failed from EBUSY";
//					// this condition might happen when a "route found" is signaled from the RE and a ReTxTimer is running:
//					// the first event calls a post sendTask(), if the ReTxTimer fires before the sendDone, the sending flag is set to false and a new sendTask is called.
//					// It's rare but it happens during loops, in particular when the current parent may select ourselves as parent (check updateRouteTask in RE)
//					// Is it a bug of CTP or of my implementation?
//
//				} else if (subsendResult == ESIZE) {
//					post_sendTask();
//				}
		}
	}

	/*
	 * The second phase of a send operation; based on whether the transmission was
	 * successful, the ForwardingEngine either stops sending or starts the
	 * RetxmitTimer with an interval based on what has occured. If the send was
	 * successful or the maximum number of retransmissions has been reached, then
	 * the ForwardingEngine dequeues the current packet. If the packet is from a
	 * client it signals Send.sendDone(); if it is a forwarded packet it returns
	 * the packet and queue entry to their respective pools.
	 *
	 */

	void event_SubSend_sendDone(block_data_t* msg, error_t error) {
		fe_queue_entry_t *qe = command_SendQueue_head();

		if (qe == NULL /*|| qe->msg != msg*/) { // cannot check the pointer since I dup the message
			echo("BUG: this should never happen");
		} else if (error != SUCCESS) { // NOT CHECKED
			// Immediate retransmission is the worst thing to do.
			echo("SubSend.sendDone - Send failed");
			startRetxmitTimer(SENDDONE_FAIL_WINDOW, SENDDONE_FAIL_OFFSET);

			//TODO: this is based on message ack, which I haven't implemented under the LE radio yet
			//must take care of this after implementing msg acking
		} else if (ackPending
				&& !command_PacketAcknowledgements_wasAcked(msg)) { // CHECK -> OK: see inner statements
				// AckPending is for case when DL cannot support acks.
//			le->command_LinkEstimator_txNoAck();
			cre.command_CtpInfo_recomputeRoutes();
			if (--qe->retries) { // CHECK -> OK: packet retxmitted after SENDDONE_NOACK_WINDOW.
				echo("SubSend.sendDone - not acked.");
				startRetxmitTimer(SENDDONE_NOACK_WINDOW, SENDDONE_NOACK_OFFSET);
			} else { // CHECK -> OK: see inner statements
				//max retries, dropping packet
				echo("Tx Dropped - max retries");
				clientPtrs = qe;

				command_SendQueue_dequeue();
				sending = false;
				startRetxmitTimer(SENDDONE_OK_WINDOW, SENDDONE_OK_OFFSET);
			}
		} else { // CHECK -> OK: packet successfully txmitted and removed from queue.
				 // there was a pointer to header that was never used... i have removed it.

			//TODO: Enable acking when implemented
//			le->command_LinkEstimator_txAck(command_AMPacket_destination(msg));
			clientPtrs = qe;
			command_SentCache_insert(qe->msg);
			command_SendQueue_dequeue();
			sending = false;
			startRetxmitTimer(SENDDONE_OK_WINDOW, SENDDONE_OK_OFFSET);
		}
	}
// ----------------------------------------------------------------------------------

//TODO: Implement data packet acknowledgement
	bool command_PacketAcknowledgements_wasAcked(block_data_t *msg) {
		//dummy
		return true;
	}

// SendQueue Interface -------------------------------------------------------
	size_t command_SendQueue_maxSize() {
		return sendqueue.max_size();
	}

	bool command_SendQueue_empty() {
		return sendqueue.empty();
	}

	error_t command_SendQueue_enqueue(fe_queue_entry_t* qe) {
		if (!sendqueue.full()) {
			sendqueue.push(qe);
			return SUCCESS;
		} else
			return ERR_BUSY;
	}

	size_t command_SendQueue_size() {
		return sendqueue.size();
	}

	fe_queue_entry_t* command_SendQueue_dequeue() {
		fe_queue_entry_t* ptr = sendqueue.front();
		sendqueue.pop();
		return ptr;
	}

	fe_queue_entry_t* command_SendQueue_element(uint8_t idx) {
		SendQueue searchQueue;
		SendQueue bkpQueue;
		fe_queue_entry_t* result;

		while (sendqueue.size() != 0) {
			searchQueue.push(sendqueue.front());
			bkpQueue.push(sendqueue.front());
			sendqueue.pop();
		}

		while (bkpQueue.size() != 0) {
			sendqueue.push(bkpQueue.front());
			bkpQueue.pop();
		}

		for (int i = 0; i < idx; i++) {
			searchQueue.pop();
		}

		result = searchQueue.front();

		while (searchQueue.size() != 0) {
			searchQueue.pop();
		}
		return result;
	}

	fe_queue_entry_t* command_SendQueue_head() {
		return sendqueue.front();
	}
// ------------------------------------------------------------------------

	void command_SentCache_insert(block_data_t* pkt) {
		if ((uint8_t) sentCache.size() == sentCacheSize) { // remove the oldest element
			SentCacheIterator eraseLocation = sentCache.begin();
			sentCache.erase(eraseLocation);
		}
		sentCache.push_back(pkt);
	}

	bool command_SentCache_lookup(block_data_t* pkt) {
		int size = sentCache.size();
		for (int i = 0; i < size; i++) {
			if (command_CtpPacket_matchInstance(pkt, sentCache[i])) {
				return true;
			}
		}
		return false;
	}

	void command_SentCache_flush() {
		sentCache.clear();
	}

}
;

// ----------------------------------------------------------------------------------

//	template<typename OsModel_P, typename RoutingTable_P,
//			typename RandomNumber_P, typename Radio_P, typename Timer_P,
//			typename Debug_P, typename Clock_P> const char * CtpForwardingEngine<
//			OsModel_P, RoutingTable_P, RandomNumber_P, Radio_P, Timer_P,
//			Debug_P, Clock_P>::timeout_period_message_[] = { "BEACON_TIMER",
//			"ROUTE_TIMER", "POST_UPDATEROUTETASK", "POST_SENDBEACONTASK" };

// ----------------------------------------------------------------------------------

}

#endif /* __CTP_FORWARDING_ENGINE_H__ */
