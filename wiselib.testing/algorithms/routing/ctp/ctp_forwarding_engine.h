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
#include "algorithms/routing/ctp/ctp_forwarding_engine_msg.h"
#include "algorithms/routing/ctp/ctp_types.h"
#include "algorithms/routing/ctp/ctp_ack_msg.h"
#include "algorithms/routing/ctp/ctp_debugging.h"

namespace wiselib {

/**
 * \brief Forwarding Engine Module
 *
 * The main task of the Forwarding Engine consists in forwarding data packets received from neighboring nodes as well as sending packets generated by the application module of the node.
 * Additionally, the FE is responsible for recognizing the occurrence of duplicate packets and routing loops and trying to fix them.
 *
 * @ingroup routing_concept
 */

template<typename OsModel_P, typename DataMessage_P, typename SendQueueValue_P,
		typename SendQueue_P, typename EntryPool_P, typename MessagePool_P,
		typename SentCache_P, typename RandomNumber_P, typename RoutingEngine_P,
		typename LinkEstimator_P, typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Debug_P = typename OsModel_P::Debug,
		typename Clock_P = typename OsModel_P::Clock>
class CtpForwardingEngine: public RoutingBase<OsModel_P, Radio_P> {
public:
	typedef OsModel_P OsModel;
	typedef DataMessage_P DataMessage;
	typedef SendQueueValue_P SendQueueValue;
	typedef SendQueue_P SendQueue;
	typedef EntryPool_P EntryPool;
	typedef MessagePool_P MessagePool;
	typedef SentCache_P SentCache;
	typedef RandomNumber_P RandomNumber;
	typedef RoutingEngine_P RoutingEngine;
	typedef LinkEstimator_P LinkEstimator;
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

	typedef typename SentCache::iterator SentCacheIterator;

	typedef CtpForwardingEngine<OsModel, DataMessage, SendQueueValue, SendQueue,
			EntryPool, MessagePool, SentCache, RandomNumber, RoutingEngine,
			LinkEstimator, Radio, Timer, Debug> self_type;
	typedef self_type* self_pointer_t;

	typedef CtpAckMsg<OsModel, Radio> AckMsg;

	typedef delegate3<void, node_id_t, size_t, block_data_t*> radio_delegate_t;
	typedef vector_static<OsModel, radio_delegate_t, RADIO_BASE_MAX_RECEIVERS> RecvCallbackVector;
	typedef typename RecvCallbackVector::iterator RecvCallbackVectorIterator;

	// --------------------------------------------------------------------

	enum ErrorCodes {
		//Default return value of success.
		SUCCESS = OsModel::SUCCESS,
		//Out of memory.
		ERR_NOMEM = OsModel::ERR_NOMEM,
		//Unspecified error value - if no other fits.
		ERR_UNSPEC = OsModel::ERR_UNSPEC,
		//Function not implemented.
		ERR_NOTIMPL = OsModel::ERR_NOTIMPL,
		//Device or resource busy - try again later.
		ERR_BUSY = OsModel::ERR_BUSY,
		//Network is down.
		ERR_NETDOWN = OsModel::ERR_NETDOWN,
		//No route to host.
		ERR_HOSTUNREACH = OsModel::ERR_HOSTUNREACH
	};

	// --------------------------------------------------------------------

	enum StateValues {
		//Ready for asking for data.
		READY = OsModel::READY,
		//Currently no data available.
		NO_VALUE = OsModel::NO_VALUE,
		//Currently inactive - so no values available.
		INACTIVE = OsModel::INACTIVE
	};

	// --------------------------------------------------------------------

	enum SpecialNodeIds {
		BROADCAST_ADDRESS = Radio_P::BROADCAST_ADDRESS, ///< All nodes in communication range
		NULL_NODE_ID = Radio_P::NULL_NODE_ID ///< Unknown/No node id
	};

	// --------------------------------------------------------------------

	enum Restrictions {
		MAX_MESSAGE_LENGTH = Radio_P::MAX_MESSAGE_LENGTH
				- DataMessage::HEADER_SIZE, ///< Maximum message length for the upper layer
	};

	// --------------------------------------------------------------------

	CtpForwardingEngine() {
	}

	// --------------------------------------------------------------------

	~CtpForwardingEngine() {
#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self_);
		echo("Re: Destroyed\n");
#endif
	}

	// --------------------------------------------------------------------

    ///@name Routing Concept methods
	///@{

	int init(void) {

		init_variables();

		return SUCCESS;
	}

	// --------------------------------------------------------------------

	int init(Radio& radio, Timer& timer, Debug& debug, Clock& clock,
			RandomNumber& random_number, RoutingEngine& re, LinkEstimator& le) {
		radio_ = &radio;
		timer_ = &timer;
		debug_ = &debug;
		clock_ = &clock;
		random_number_ = &random_number;
		re_ = &re;
		le_ = &le;

		init_variables();

		return SUCCESS;
	}

	// --------------------------------------------------------------------

	int destruct(void) {
		return disable_radio();
	}

	// --------------------------------------------------------------------

	int enable_radio(void) {

		radio().template reg_recv_callback<self_type, &self_type::receive>(
				this);

		re_->template reg_listener_callback<self_type, &self_type::rcv_event>(
				this);

		start();
		return radio().enable_radio();
	}

	// --------------------------------------------------------------------

	int disable_radio(void) {
		stop();
		return radio().disable_radio();
	}

	// ----------------------------------------------------------------------------------

	node_id_t id() {
		return radio_->id();
	}

	// --------------------------------------------------------------------

	int send(node_id_t destination, size_t len, block_data_t *data) {

		SendQueueValue* qe;
		DataMessage* msg;

		if (!running_) {
			return ERR_BUSY;
		}

		if (len > MAX_MESSAGE_LENGTH) {
			return ERR_NOTIMPL;
		}

		if (entrypool_.empty()) {
			echo("Can't send. Entry pool empty.");
			return ERR_BUSY;
		}

		if (msgpool_.empty()) {
			echo("Can't send. Message pool empty.");
			return ERR_BUSY;
		}

		msg = &msgpool_.front();
		msg->set_msg_id(CtpDataMsgId);
		msg->set_options(0);
		msg->set_pull();
		msg->set_origin(self_);
		msg->set_seqno(seqno_++);
		msg->set_thl(0);
		msg->set_payload(data, len);

		qe = &entrypool_.front();
		qe->msg = msg;
		qe->len = len + DataMessage::HEADER_SIZE;
		qe->retries = MAX_RETRIES;

		echo("message %s from %x to %x", data, self_, re_->get_next_hop());

		if (send_queue_enqueue(qe) == SUCCESS) {
			if (!retx_timer_is_running_) {
				program_send_task();
			}
			return SUCCESS;
		} else {
			echo("Send.send - Send failed as packet could not be enqueued.");
			return ERR_UNSPEC;
		}
	}

	// --------------------------------------------------------------------

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

	///@}

	// --------------------------------------------------------------------

	///@name CTP Specific methods
	///@{

	// A simple predicate to determine congestion state of this node.
	bool is_congested() {
		return send_queue_size() > congestion_threshold_;
	}

	///@}
	// --------------------------------------------------------------------

private:

	enum {
		MAX_RETRIES = 30
	};

	enum TimerPeriods {
		FORWARD_PACKET_TIME = 4,
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
		RETX_PERIOD = 10000 //ms
	};

	// --------------------------------------------------------------------

	enum TimerId {
		RETXTIMER = 1, CONGESTION_TIMER = 2, POST_SENDTASK = 3
	};

	// --------------------------------------------------------------------

	enum SentCacheLookupResults {
		///The message was not in the sent cache
		NO_MATCH = 0,
		///This exact message instance was found in the sent cache => duplicate message
		FULL_MATCH = 1,
		///Another instance of the same message has been found
		///This means the message has passed through the node before already but has a different THL
		///It may indicate a recent change in the topology/ETX values that causes the message to float around until a new route is found
		PARTIAL_MATCH = 2
	};

	// --------------------------------------------------------------------

	typename Radio::self_pointer_t radio_;
	typename Timer::self_pointer_t timer_;
	typename Debug::self_pointer_t debug_;
	typename Clock::self_pointer_t clock_;
	typename RandomNumber::self_pointer_t random_number_;
	typename RoutingEngine::self_pointer_t re_;
	typename LinkEstimator::self_pointer_t le_;

	// --------------------------------------------------------------------

	uint8_t congestion_threshold_;
	bool running_;
	node_id_t last_parent_;
	uint8_t seqno_;

	///queue of pointers to messages to be sent
	SendQueue send_queue_;

	///pool of preallocated containers for send_queue_ entries
	EntryPool entrypool_;
	///pool of preallocated containers for data messages
	MessagePool msgpool_;

	/// Required by our implementation of timers.
	bool congestion_timer_is_running_;
	bool retx_timer_is_running_;
	bool send_task_timer_is_running_;

	///Holds the last few sent messages for quicker loop detection
	SentCache sent_cache_;

	/// Node own ID
	node_id_t self_;

	RecvCallbackVector recv_callbacks_;

	// ----------------------------------------------------------------------------------

    ///@name Startup
	///@{

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
		SendQueueValue dummy_entry;
		DataMessage dummy_msg(CtpDataMsgId);

		self_ = radio().id();

		// Since we don't know whether a timer is running, we put these bool values
		// to true when the timer is launched and to false when it fires.
		congestion_timer_is_running_ = false;
		retx_timer_is_running_ = false;
		send_task_timer_is_running_ = false;

		running_ = false;

		congestion_threshold_ = send_queue_maxsize() >> 1;
		last_parent_ = NULL_NODE_ID; //last parent we sent to
		seqno_ = 0;

		//preallocate memory for the send queue entries
		while (!entrypool_.full()) {
			entrypool_.push(dummy_entry);
		}

		//preallocate memory for the messages
		while (!msgpool_.full()) {
			msgpool_.push(dummy_msg);
		}
	}

	// ----------------------------------------------------------------------------------

	error_t start() {
		running_ = true;
		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------

	error_t stop() {
		running_ = false;
		return SUCCESS;
	}

	///@}

	// ----------------------------------------------------------------------------------

    ///@name Incoming and Outgoing Event Management
	///@{

	void notify_receivers(node_id_t from, size_t len, block_data_t *data) {
		for (RecvCallbackVectorIterator it = recv_callbacks_.begin();
				it != recv_callbacks_.end(); ++it) {
			if (*it != radio_delegate_t())
				(*it)(from, len, data);
		}
	}

	// ----------------------------------------------------------------------------------

	/**
	 * Received a message to forward. Check whether it is a duplicate by
	 * checking the packets currently in the queue as well as the
	 * send history cache (in case we recently forwarded this packet).
	 * The cache is important as nodes immediately forward packets
	 * but wait a period before retransmitting.
	 * If this node is a root, signal receive.
	 */
	void receive(node_id_t from, size_t len, block_data_t *data) {

		if (!running_ || from == radio().id()) {
			return;
		}

		message_id_t msg_id = read<OsModel, block_data_t, message_id_t>(data);

//			echo("Received message with id = %d from %x",from);

		if (msg_id == CtpAckMsgId) {

			process_ack_message(from, data);

		} else if (msg_id == CtpDataMsgId) {

			DataMessage* msg = reinterpret_cast<DataMessage*>(data);
			SentCacheLookupResults cacheResult;

			//echo("Received msg %s from %x",msg->payload(),from);

			// Update the THL here, since it has lived another hop, and so
			// that the root sees the correct THL.

			 uint8_t thl = msg->thl();
			thl++;
			msg->set_thl(thl);

			//See if we remember having seen this packet
			//We look in the sent cache ...
			cacheResult = sent_cache_lookup(msg);
			if (cacheResult == FULL_MATCH) {
				//Message instance duplicate -> discard
//					echo("Discarding duplicate sent_cache_ msg = %s, seqno = %d, thl = %d from %d", msg->payload(), msg->seqno(), msg->thl(), from);
				return;
			} else if (cacheResult == PARTIAL_MATCH) {
				//The message has passed through the node before
				//It is floating around waiting for a route to be found
				//Speed up the process by forcing the RE to recompute routes
				re_->trigger_route_update();
			}

			//... and in the queue for duplicates

			if (send_queue_contains(msg)) {
//				echo("Discarding duplicate sendQueue msg = %s, seqno = %d, thl = %d from %x",msg->payload(), msg->seqno(), msg->thl(), from);
				return;
			}

			if (msg->pull()) {
				re_->schedule_route_update();
			}

			re_->set_neighbor_congested(from, msg->congestion());

			//Send back ack msg
			AckMsg ack_msg(CtpAckMsgId);
			ack_msg.set_origin(msg->origin());
			ack_msg.set_seqno(msg->seqno());

			radio().send(from, AckMsg::HEADER_SIZE,
					reinterpret_cast<block_data_t*>(&ack_msg));

			if (re_->is_root()) {

				// If I'm the root, signal receive.
				notify_receivers(from, len - DataMessage::HEADER_SIZE,
						msg->payload());
				return;

			} else {
				echo("message %s from %x to %x", msg->payload(), self_,
						re_->get_next_hop());
				forward(len, msg);
			}
		}
	}

	// ----------------------------------------------------------------------------------

	void rcv_event(uint8_t event) {
		switch (event) {
		case (RoutingEngine::RE_EVENT_ROUTE_FOUND):
			echo("Route found!");
			program_send_task();
			break;
		case (RoutingEngine::RE_EVENT_ROUTE_NOT_FOUND):
			echo("No Route!");
			break;
		default:
			break;
		}
	}

	///@}

	// ------------------------------------------------------------------------

    ///@name Timer Interface
	///@{

	int set_timer(void *userdata, millis_t millis) {
		int timeout = (int) (userdata);

		switch (timeout) {

		case RETXTIMER:

			if (retx_timer_is_running_) {
				//echo("retx_timer_is_running_");
				return ERR_BUSY;
			}

			retx_timer_is_running_ = true;
			break;

		case CONGESTION_TIMER:

			if (retx_timer_is_running_) {
				//echo("congestion_timer_is_running_");
				return ERR_BUSY;
			}

			congestion_timer_is_running_ = true;
			break;

		case POST_SENDTASK:

			if (send_task_timer_is_running_) {
				//echo("send_task_timer_is_running_");
				return ERR_BUSY;
			}

			send_task_timer_is_running_ = true;
			break;
		}
		return timer().template set_timer<self_type, &self_type::timer_elapsed>(
				millis, this, userdata);
	}

	// ----------------------------------------------------------------------------------

	void timer_elapsed(void *userdata) {
		int timeout = (int) (userdata);

		switch (timeout) {

		case RETXTIMER:
			retx_timer_is_running_ = false;
			retx_timer_fired();
			break;

		case CONGESTION_TIMER:
			congestion_timer_is_running_ = false;
			congestion_timer_fired();
			break;

		case POST_SENDTASK:
			send_task_timer_is_running_ = false;
			send_task();
			break;

		default:
#ifdef FORWARDING_ENGINE_DEBUG
			echo("%d: ", self_);
			echo("Re: TimerFiredCallback unexpected timeout: %d\n",
					timeout);
#endif
			break;
		}
	}

	// ----------------------------------------------------------------------------------

	void start_retx_timer(uint16_t mask, uint16_t offset) {
		uint16_t r = random_number().short_rand();
		r &= mask;
		r += offset;

		if (!retx_timer_is_running_) {
			set_timer((void*) RETXTIMER, r);
		}
	}

	// ----------------------------------------------------------------------------------

	void start_congestion_timer(uint16_t mask, uint16_t offset) {
		uint16_t r = random_number().short_rand();
		r &= mask;
		r += offset;

		if (!congestion_timer_is_running_) {
			set_timer((void*) CONGESTION_TIMER, r);
		}
	}

	// ----------------------------------------------------------------------------------

	void retx_timer_fired() {
		program_send_task();
	}

	// ----------------------------------------------------------------------------------

	void congestion_timer_fired() {
		program_send_task();
	}

	///@}

	// ----------------------------------------------------------------------------------

    ///@name Message Handling
	///@{

	/**
	 * Function for preparing a packet for forwarding. Performs
	 * a buffer swap from the message pool. If there are no free
	 * message in the pool, it returns the passed message and does not
	 * put it on the send queue.
	 */
	void forward(size_t len, DataMessage* msg) {
		SendQueueValue *qe;
		ctp_etx_t gradient;
		DataMessage* poolMsg;

		if (entrypool_.empty()) {
			echo("Can't forward. Entry pool empty.");
			return;
		}

		qe = &entrypool_.front();

		poolMsg = &msgpool_.front();

		memcpy(poolMsg, msg, sizeof(DataMessage));

		qe->msg = poolMsg;
		qe->retries = MAX_RETRIES;
		qe->len = len;

		if (send_queue_enqueue(qe) == SUCCESS) {

			// Loop-detection code:
			if (re_->get_etx(&gradient) == SUCCESS) {
				// We only check for loops if we know our own metric
				if (msg->etx() <= gradient) {
					// If our etx metric is less than or equal to the etx value
					// on the packet (etx of the previous hop node), then we believe
					// we are in a loop.
					// Trigger a route update and backoff.
					echo("Possible Loop Detection...");
					re_->schedule_route_update();
					start_retx_timer(LOOPY_WINDOW, LOOPY_OFFSET);
				}
			}

			if (!retx_timer_is_running_) {
				// send_task is only immediately posted if we don't detect a
				// loop.
				program_send_task();
			}

			// Successful function exit point:
			return;
		} else {
			echo("There was a problem enqueuing to the send queue.");
		}
	}

	// ----------------------------------------------------------------------------------

	void process_ack_message(node_id_t from, block_data_t *msg) {

		if (!send_queue_empty() && msg != NULL) {

			SendQueueValue *qe = send_queue_head();
			AckMsg* ack_msg = reinterpret_cast<AckMsg*>(msg);

			if (qe == NULL) {
				echo("BUG: this should never happen");
				return;
			}

			if (qe->msg->origin() == ack_msg->origin()
					&& qe->msg->seqno() == qe->msg->seqno()) {

				// Packet successfully txmitted
				//echo("Message %s was acked by %d",qe->msg->payload(),from);
				le_->ack_received(re_->get_next_hop());
				sent_cache_insert(qe->msg);
				send_queue_dequeue();
			}
		}

	}

	// ----------------------------------------------------------------------------------

	/**
	 *  Schedule the send_task function as soon as possible.
	 *  Note that it is not possible to directly call the function
	 *  because of the infinite recursion in the send_task function.
	 */
	void program_send_task() {
		if (!send_task_timer_is_running_) {
			set_timer((void*) POST_SENDTASK, 0);
		}
	}

	// ----------------------------------------------------------------------------------

	void send_task() {

		if (send_queue_empty()) {
			// When queue empty do nothing.
			return;
		}

		if (!re_->is_root() && !re_->has_route()) {

			// retx called

			echo("No route available. retry after 10s");
			if (!retx_timer_is_running_) {
				set_timer((void*) RETXTIMER, RETX_PERIOD);
			}

			return;

		} else {

			error_t subsendResult;
			SendQueueValue* qe = send_queue_head();
			node_id_t dest = re_->get_next_hop();
			ctp_etx_t gradient;

			if (re_->is_neighbor_congested(dest)) { // NOT CHECKED
				// Our parent is congested. We should wait.
				// Don't repost the task, CongestionTimer will do the job
				echo("Parent %x congested. Wait....", dest);
				if (!congestion_timer_is_running_) {
					start_congestion_timer(CONGESTED_WAIT_WINDOW,
							CONGESTED_WAIT_OFFSET);
				}
				return;
			}

			// Once we are here, we have decided to send the packet.
			if (sent_cache_lookup(qe->msg) == FULL_MATCH) { // NOT CHECKED
				send_queue_dequeue();
				//We must post the task instead of calling it directly
				//to avoid infinite recursion
				program_send_task();
				return;
			}

			/* If our current parent is not the same as the last parent
			 we sent do, then reset the count of unacked packets: don't
			 penalize a new parent for the failures of a prior one.*/
			if (dest != last_parent_) { // CHECK -> OK: qe retries initialized
				qe->retries = MAX_RETRIES;
				last_parent_ = dest;
			}

			if (re_->is_root()) { // CHECK -> OK: loppbacked message to app layer.

				SendQueueValue* entry = send_queue_head();

				notify_receivers(self_, entry->len, entry->msg->payload());

				send_queue_dequeue();

				echo("send_task - I'm root, so loopback and signal receive.");

				return;
			}

			// Loop-detection functionality:
			if (re_->get_etx(&gradient) != SUCCESS) { // NOT CHECKED
				// If we have no metric, set our gradient conservatively so
				// that other nodes don't automatically drop our packets.
				gradient = 0;
			}

			qe->msg->set_etx(gradient);

			// Set or clear the congestion bit on *outgoing* packets.
			if (is_congested()) {
				echo("I am congested");
				qe->msg->set_congestion();
			} else {
				qe->msg->clear_congestion();
			}

			subsendResult = radio().send(dest, qe->len,
					reinterpret_cast<block_data_t*>(qe->msg));

			//print_send_queue();
//				echo("Sending message %s to %d ", qe->msg->payload(), dest);

			/*
			 * The second phase of a send operation; based on whether the transmission was
			 * successful, the ForwardingEngine either stops sending or starts the
			 * RetxmitTimer with an interval based on what has occured. If the send was
			 * successful or the maximum number of retransmissions has been reached, then
			 * the ForwardingEngine dequeues the current packet. If the packet is from the
			 * application it signals Send.sendDone(); if it is a forwarded packet it returns
			 * the packet and queue entry to their respective pools.
			 *
			 */

			if (subsendResult != SUCCESS) { // NOT CHECKED
				// Immediate retransmission is the worst thing to do.
				echo("SubSend.sendDone - Send failed");
				start_retx_timer(SENDDONE_FAIL_WINDOW, SENDDONE_FAIL_OFFSET);

			} else {

				if (qe->retries-- == MAX_RETRIES) {

					//First time we're sending this message
#ifdef SHAWN
					if (!retx_timer_is_running_) {
						set_timer((void*) RETXTIMER, 2200);
					}
#else
					start_retx_timer(SENDDONE_OK_WINDOW, SENDDONE_OK_OFFSET);
#endif
				} else if (qe->retries > 0) {

					//Have tried to send this message before but didn't receive any ack

					le_->ack_not_received(dest);
					re_->trigger_route_update();

					//echo("Message %s not acked.",qe->msg->payload());

#ifdef SHAWN
					if (!retx_timer_is_running_) {
						set_timer((void*) RETXTIMER, 2200);
					}
#else
					start_retx_timer(SENDDONE_NOACK_WINDOW,
							SENDDONE_NOACK_OFFSET);
#endif
				} else {

					//Max retries, message wasn't acked

					echo("Msg %s Dropped - max retries", qe->msg->payload());

					send_queue_dequeue();
					sent_cache_insert(qe->msg);

					re_->schedule_route_update();

					start_retx_timer(SENDDONE_OK_WINDOW, SENDDONE_OK_OFFSET);

				}

			}
		}
	}

	// ----------------------------------------------------------------------------------

	/**
	 * A CTP packet ID is based on the origin, seqno and the THL fields, to implement duplicate suppression
	 */
	bool match_message_instance(DataMessage* msg1, DataMessage* msg2) {
		return (msg1->origin() == msg2->origin()
				&& msg1->seqno() == msg2->seqno() && msg1->thl() == msg2->thl());
	}

	///@}

	// ----------------------------------------------------------------------------------

    ///@name Send Queue Interface
	///@{

	size_t send_queue_maxsize() {
		return send_queue_.max_size();
	}

	// ----------------------------------------------------------------------------------

	bool send_queue_empty() {
		return send_queue_.empty();
	}

	// ----------------------------------------------------------------------------------

	error_t send_queue_enqueue(SendQueueValue* qe) {
		if (!send_queue_.full()) {
			send_queue_.push(qe);
			entrypool_.pop();
			msgpool_.pop();
			re_->update_congested_state(is_congested());
			return SUCCESS;
		} else
			return ERR_BUSY;
	}

	// ----------------------------------------------------------------------------------

	size_t send_queue_size() {
		return send_queue_.size();
	}

	// ----------------------------------------------------------------------------------

	SendQueueValue* send_queue_dequeue() {
		SendQueueValue* ptr = send_queue_.front();
		send_queue_.pop();
		entrypool_.push(*ptr);
		msgpool_.push(*(ptr->msg));
		re_->update_congested_state(is_congested());
		return ptr;
	}

	// ----------------------------------------------------------------------------------

	bool send_queue_contains(DataMessage* msg) {
		SendQueue backup_queue;
		SendQueueValue* qe;
		bool result = false;

		while (send_queue_.size() != 0) {
			backup_queue.push(send_queue_.front());
			qe=send_queue_.front();
			if (match_message_instance(msg,qe->msg)) {
				result=true;
			}
			send_queue_.pop();
		}

		while (backup_queue.size() != 0) {
			send_queue_.push(backup_queue.front());
			backup_queue.pop();
		}

		return result;
	}

	// ----------------------------------------------------------------------------------

	SendQueueValue* send_queue_head() {
		return send_queue_.front();
	}

	// ----------------------------------------------------------------------------------

	void print_send_queue() {
		SendQueue backup_queue;
		SendQueueValue* temp;

		echo("SendQueue: ");

		while (send_queue_.size() != 0) {
			temp = send_queue_.front();
			backup_queue.push(send_queue_.front());
			send_queue_.pop();

			echo("msg = %s, retries = %d, seqno_ = %d", temp->msg->payload(),
					temp->retries, temp->msg->seqno());
		}

		while (backup_queue.size() != 0) {
			send_queue_.push(backup_queue.front());
			backup_queue.pop();
		}
	}

	///@}

	// ------------------------------------------------------------------------

    ///@name Sent Cache Interface
	///@{

	void sent_cache_insert(DataMessage* msg) {
		//echo("Inserting %s into the sent_cache_",msg->payload());
		if (sent_cache_.size() == sent_cache_.max_size()) {
			// remove the oldest element
			sent_cache_.erase(sent_cache_.begin());
		}
		sent_cache_.push_back(msg);
	}

	// ------------------------------------------------------------------------

	SentCacheLookupResults sent_cache_lookup(DataMessage* msg) {
		int size = sent_cache_.size();
		for (int i = 0; i < size; i++) {
			if (match_message_instance(msg, sent_cache_[i])) {
				//echo("send_task: Message %s with seqno=%d found in the sent cache",msg->payload(),msg->seqno());
//					print_sent_cache();
				return FULL_MATCH;
			} else if ((msg->origin() == sent_cache_[i]->origin())
					&& (msg->seqno() == sent_cache_[i]->seqno())) {
				return PARTIAL_MATCH;
			}
		}
		return NO_MATCH;
	}

	// ------------------------------------------------------------------------

	void print_sent_cache() {
		int size = sent_cache_.size();
		echo("SentCache: ");
		for (int i = 0; i < size; i++) {
			echo("message: %s, origin = %x, thl = %d, seqno = %d",
					sent_cache_[i]->payload(), sent_cache_[i]->origin(),
					sent_cache_[i]->thl(), sent_cache_[i]->seqno());
		}
	}

	///@}

	// ----------------------------------------------------------------------------------

    ///@name Debugging Interface
	///@{

	void echo(const char *msg, ...) {
		va_list fmtargs;
		char buffer[1024];
		int i;
		for (i = 0; i < DEBUG_NODES_NR; i++) {
			if (id() == nodes[debug_nodes[i]]) {
				va_start(fmtargs, msg);
				vsnprintf(buffer, sizeof(buffer) - 1, msg, fmtargs);
				va_end(fmtargs);
#ifdef SHAWN
				debug().debug("%d: FE: ", id());
#endif
				debug().debug(buffer);
#ifdef SHAWN
				debug().debug("\n");
#endif
				break;
			}
		}
	}

	///@}

}
;

}

#endif /* __CTP_FORWARDING_ENGINE_H__ */
