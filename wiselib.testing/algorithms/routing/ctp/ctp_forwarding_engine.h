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

	template<typename OsModel_P, typename DataMessage_P, typename SendQueueValue_P, typename SendQueue_P,
		typename EntryPool_P, typename MessagePool_P, typename SentCache_P, typename RandomNumber_P, typename RoutingEngine_P,
		typename Radio_P = typename OsModel_P::Radio,
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

		typedef SendQueueValue fe_queue_entry_t;
		typedef typename SentCache::iterator SentCacheIterator;

		typedef CtpForwardingEngine<OsModel, DataMessage,SendQueueValue, SendQueue,EntryPool,MessagePool, SentCache,
			RandomNumber, RoutingEngine, Radio, Timer, Debug> self_type;
		typedef self_type* self_pointer_t;

		typedef delegate3<void, node_id_t, size_t, block_data_t*> radio_delegate_t;
		typedef vector_static<OsModel, radio_delegate_t, RADIO_BASE_MAX_RECEIVERS> RecvCallbackVector;
		typedef typename RecvCallbackVector::iterator RecvCallbackVectorIterator;

		typedef delegate1<void, uint8_t> event_delegate_t;
		typedef vector_static<OsModel, event_delegate_t, FE_MAX_EVENT_RECEIVERS> EventCallbackVector;
		typedef typename EventCallbackVector::iterator EventCallbackVectorIterator;

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
			MAX_MESSAGE_LENGTH = Radio_P::MAX_MESSAGE_LENGTH - DataMessage::HEADER_SIZE, ///< Maximum message length for the upper layer
		};

		// --------------------------------------------------------------------


		CtpForwardingEngine() {
		}

		// --------------------------------------------------------------------

		~CtpForwardingEngine() {
#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self);
			echo("Re: Destroyed\n");
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
				cre = &re;

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

			radio().template reg_recv_callback<self_type, &self_type::receive>(
				this);

			cre->template reg_event_callback<self_type, &self_type::rcv_event>(
					this);

			command_StdControl_start();
			return radio().enable_radio();
		}

		// --------------------------------------------------------------------

		int disable_radio(void) {
			command_StdControl_stop();
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
		uint8_t reg_event_callbfack(T *obj_pnt) {

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

		// A simple predicate for now to determine congestion state of this node.
		bool command_CtpCongestion_isCongested() {
			return command_SendQueue_size() > congestionThreshold;
		}

	private:

		enum {
			MAX_RETRIES = 30
		};

		enum TimerPeriods {
			FORWARD_PACKET_TIME =			4,
			SENDDONE_FAIL_OFFSET =			512,
			SENDDONE_NOACK_OFFSET =			FORWARD_PACKET_TIME << 2,
			SENDDONE_OK_OFFSET =			FORWARD_PACKET_TIME << 2,
			LOOPY_OFFSET =					FORWARD_PACKET_TIME << 4,
			SENDDONE_FAIL_WINDOW =			SENDDONE_FAIL_OFFSET - 1,
			LOOPY_WINDOW =					LOOPY_OFFSET - 1,
			SENDDONE_NOACK_WINDOW =			SENDDONE_NOACK_OFFSET - 1,
			SENDDONE_OK_WINDOW =			SENDDONE_OK_OFFSET - 1,
			CONGESTED_WAIT_OFFSET =			FORWARD_PACKET_TIME << 2,
			CONGESTED_WAIT_WINDOW =			CONGESTED_WAIT_OFFSET - 1,
			RETX_PERIOD =					10000 //ms
		};

		// --------------------------------------------------------------------

		enum TimerIds {
			RETXTIMER = 1, CONGESTION_TIMER = 2, POST_SENDTASK = 3
		};

		// --------------------------------------------------------------------

		typedef enum {
			//The message was not in the sent cache
			NO_MATCH = 0, 
			//This exact message instance was found in the sent cache => duplicate message
			FULL_MATCH = 1, 
			//Another instance of the same message has been found
			//This means the message has passed through the node before already but has a different THL
			//This may point to a recent change in the topology/ETX values that causes the message to float around until a new route is found
			PARTIAL_MATCH = 2 
		} cache_lookup_result_t;

		// --------------------------------------------------------------------

		typename Radio::self_pointer_t radio_;
		typename Timer::self_pointer_t timer_;
		typename Debug::self_pointer_t debug_;
		typename Clock::self_pointer_t clock_;
		typename RandomNumber::self_pointer_t random_number_;

		// --------------------------------------------------------------------

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

		//queue of pointers to messages to be sent
		SendQueue sendqueue;
		
		//pool of preallocated containers for sendqueue entries
		EntryPool entrypool_;
		//pool of preallocated containers for data messages
		MessagePool msgpool_;

		// Required by our implementation of timers.
		bool congestionTimerIsRunning;
		bool reTxTimerIsRunning;

		//Holds the last few sent messages for quicker loop detection
		SentCache sentCache;

		typename RoutingEngine::self_pointer_t cre;

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
			fe_queue_entry_t dummy_entry;
			DataMessage dummy_msg(CtpDataMsgId);


			self = radio().id();

			//TODO: Pass configuration values as template parameter

			// Since we don't know whether a timer is running, we put these bool values
			// to true when the timer is launched and to false when it fires.
			congestionTimerIsRunning = false;
			reTxTimerIsRunning = false;

			parentCongested = false;
			running = false;
			radioOn = true; // TO IMPLEMENT ------------- radioOn must be off then turned on when radio control event is triggered
			ackPending = false;
			sending = false;

			clientPtrs = &clientEntries; // we support one client only -> we don't use an array
			congestionThreshold = command_SendQueue_maxSize() >> 1;
			lastParent = NULL_NODE_ID; //last parent we sent to
			seqno = 0;

			//preallocate memory for the send queue entries
			while (!entrypool_.full()) {
				entrypool_.push(dummy_entry);
			}

			//preallocate memory for the messages
			while (!msgpool_.full()) {
				msgpool_.push(dummy_msg);
			}
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
					echo("%d: ", self);
					echo("Re: TimerFiredCallback unexpected timeout: %d\n",
						timeout);
					return;
#endif
					break;
				}
								}
			}
		}
		// ----------------------------------------------------------------------------------

		error_t command_StdControl_start(){
			running = true ;
			return SUCCESS ;
		}

		error_t command_StdControl_stop(){
			running = false ;
			return SUCCESS ;
		}

		/*
		* Function for preparing a packet for forwarding. Performs
		* a buffer swap from the message pool. If there are no free
		* message in the pool, it returns the passed message and does not
		* put it on the send queue.
		*/
		void forward(size_t len, DataMessage* msg) {
			fe_queue_entry_t *qe;
			ctp_etx_t gradient;
			DataMessage* poolMsg;

			if (entrypool_.empty()) {
				echo("Can't forward. Entry pool empty.");
				return;
			}

			qe=&entrypool_.front();

			poolMsg = &msgpool_.front();

			memcpy(poolMsg,msg,sizeof(DataMessage));
			
			

			qe->msg = poolMsg;
			qe->retries = MAX_RETRIES;
			qe->len=len;

			if (command_SendQueue_enqueue(qe) == SUCCESS) {

				// Loop-detection code:
				if (cre->command_CtpInfo_getEtx(&gradient) == SUCCESS) {
					// We only check for loops if we know our own metric
					if (msg->etx() <= gradient) {
						// If our etx metric is less than or equal to the etx value
						// on the packet (etx of the previous hop node), then we believe
						// we are in a loop.
						// Trigger a route update and backoff.
						echo("Possible Loop Detection...");
						cre->command_CtpInfo_triggerRouteUpdate();
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

			if (from == radio().id()) {
				return;
			}

#ifdef CTP_DEBUGGING
		if (getConnection(self, from)==NULL) {
			return;
		}
#endif

			DataMessage* msg = reinterpret_cast<DataMessage*>(data);
			cache_lookup_result_t cacheResult;

			if (msg->msg_id() != CtpDataMsgId) {
				return;
			}

			//echo("Received msg %s from %d",msg->payload(),from);

			bool duplicate = false;
			fe_queue_entry_t* qe;
			uint8_t i, thl;

			// Update the THL here, since it has lived another hop, and so
			// that the root sees the correct THL.
			thl = msg->thl();
			thl++;
			msg->set_thl(thl);

			//echo("Received msg = %s, origin = %d, seqno = %d, thl = %d, etx = %d, from %d",msg->payload(), msg->origin(),msg->seqno(),msg->thl(),msg->etx(),from);
			//printSentCache();

			//See if we remember having seen this packet
			//We look in the sent cache ...
			cacheResult = command_SentCache_lookup(msg);
			if (cacheResult==FULL_MATCH) {
				//Message instance duplicate -> discard
				echo("Discarding duplicate msg = %s, seqno = %d, thl = %d from %d",msg->payload(),msg->seqno(),msg->thl(),from);
				return;
			} else if (cacheResult==PARTIAL_MATCH) {
				//The message has passed through the node before
				//It is floating aroumd waiting for a route to be found
				//Speed up the process by forcing the RE to recompute routes 
				cre->command_CtpInfo_recomputeRoutes();
			}

			//... and in the queue for duplicates
			if (command_SendQueue_size() > 0) {
				for (i = command_SendQueue_size(); --i;) {
					qe = command_SendQueue_element(i);
					if (command_CtpPacket_matchInstance(qe->msg, msg)) {
						duplicate = true;
						break;
					}
				}
			}

			if (duplicate) {
				return;
			}

			// If I'm the root, signal receive.
			else if (cre->command_RootControl_isRoot()) {
				// sends the packet to application layer.
				notify_receivers(from, len - DataMessage::HEADER_SIZE,
					msg->payload());
				return;
			} else {
				forward(len,msg);
			}
		}

		// ----------------------------------------------------------------------------------

		void rcv_event(uint8_t event) {
			switch (event) {
			case (RoutingEngine::RE_EVENT_ROUTE_FOUND):
				echo("Route found!") ;
				post_sendTask() ;
				break;
			case (RoutingEngine::RE_EVENT_ROUTE_NOT_FOUND):
				echo("No Route!") ;
				break;
			default:
				break;
			}
		}

		void event_RetxmitTimer_fired() {
			sending = false;
			post_sendTask();
		}

		void event_CongestionTimer_fired() {
			post_sendTask();
		}

		// A CTP packet ID is based on the origin and the THL field, to
		// implement duplicate suppression as described in TEP 123.
		bool command_CtpPacket_matchInstance(DataMessage* msg1,
			DataMessage* msg2) {

				return (msg1->origin() == msg2->origin()
					&& msg1->seqno() == msg2->seqno() && msg1->thl() == msg2->thl());
		}

		void startRetxmitTimer(uint16_t mask, uint16_t offset) {
			uint16_t r = random_number().randShort();
			r &= mask;
			r += offset;
			setTimer((void*) RETXTIMER, r);
			reTxTimerIsRunning = true;
		}

		void startCongestionTimer(uint16_t mask, uint16_t offset) {
			uint16_t r = random_number().randShort();
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
			DataMessage* msg;

			if (!running) {
				return ERR_NOTIMPL;
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
			msg->set_origin(self);
			msg->set_seqno(seqno++);
			msg->set_thl(0);
			msg->set_payload(pkt,len);



			qe = &entrypool_.front();
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
			} else if (!cre->command_RootControl_isRoot()
				&& !cre->command_Routing_hasRoute()) { // CHECK -> OK: retx called after 10 sec.
					setTimer((void*) RETXTIMER, RETX_PERIOD);

					return;
			} else {

				error_t subsendResult;
				fe_queue_entry_t* qe = command_SendQueue_head();
				node_id_t dest = cre->command_Routing_nextHop();
				ctp_etx_t gradient;

				if (cre->command_CtpInfo_isNeighborCongested(dest)) { // NOT CHECKED
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
				if (command_SentCache_lookup(qe->msg)==FULL_MATCH) { // NOT CHECKED
					command_SendQueue_dequeue();
					echo("sendTask: Message found in the sent cache");
					//We must post the task instead of calling it directly
					//to avoid infinite recursion
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

				if (cre->command_RootControl_isRoot()) { // CHECK -> OK: loppbacked message to app layer.

					ackPending = false;

					fe_queue_entry_t* entry = command_SendQueue_head();
					
					notify_receivers(self, entry->len, entry->msg->payload());

					command_SendQueue_dequeue();

					echo("sendTask - I'm root, so loopback and signal receive.");

					return;
				}

				// Loop-detection functionality:
				if (cre->command_CtpInfo_getEtx(&gradient) != SUCCESS) { // NOT CHECKED
					// If we have no metric, set our gradient conservatively so
					// that other nodes don't automatically drop our packets.
					gradient = 0;
				}

				qe->msg->set_etx(gradient);

				ackPending = true;

				// Set or clear the congestion bit on *outgoing* packets.
				if (command_CtpCongestion_isCongested()) {
					qe->msg->set_congestion();
				} else {
					qe->msg->clear_congestion();
				}

				subsendResult = radio().send(dest, qe->len, reinterpret_cast<block_data_t*>(qe->msg));

				//printSendQueue();
				echo("Sending message %s to %d ",qe->msg->payload(),dest);

				//TODO: Implement callback to send_done event from RE
				event_SubSend_sendDone(qe->msg, subsendResult);
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

		void event_SubSend_sendDone(DataMessage* msg, error_t error) {

			if (command_SendQueue_empty()) {
				echo("queue is empty");
			}
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

					//TODO: Add when implemented acking
					//			le->command_LinkEstimator_txNoAck();
					cre->command_CtpInfo_recomputeRoutes();
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

				//TODO: Enable acking when implemented
				//			le->command_LinkEstimator_txAck(command_AMPacket_destination(msg));
				command_SentCache_insert(qe->msg);
				command_SendQueue_dequeue();
				sending = false;

				startRetxmitTimer(SENDDONE_OK_WINDOW, SENDDONE_OK_OFFSET);
			}
		}
		// ----------------------------------------------------------------------------------

		//TODO: Implement data packet acknowledgement
		bool command_PacketAcknowledgements_wasAcked(DataMessage *msg) {
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
				entrypool_.pop();
				msgpool_.pop();
				cre->command_updateCongestedState(command_CtpCongestion_isCongested());
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
			entrypool_.push(*ptr);
			msgpool_.push(*(ptr->msg));
			cre->command_updateCongestedState(command_CtpCongestion_isCongested());
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

		void printSendQueue() {
			SendQueue bkpQueue;
			fe_queue_entry_t* temp;

			echo("SendQueue: ");

			while (sendqueue.size() != 0) {
				temp=sendqueue.front();
				bkpQueue.push(sendqueue.front());
				sendqueue.pop();

				echo("msg = %s, retries = %d, seqno = %d",temp->msg->payload(),temp->retries,temp->msg->seqno());
			}

			while (bkpQueue.size() != 0) {
				sendqueue.push(bkpQueue.front());
				bkpQueue.pop();
			}
		}
		// ------------------------------------------------------------------------

		void command_SentCache_insert(DataMessage* pkt) {
			if (sentCache.size() == sentCache.max_size()) { 
				// remove the oldest element
				SentCacheIterator eraseLocation = sentCache.begin();
				sentCache.erase(eraseLocation);
			}
			sentCache.push_back(pkt);
		}

		cache_lookup_result_t command_SentCache_lookup(DataMessage* pkt) {
			int size = sentCache.size();
			for (int i = 0; i < size; i++) {
				if (command_CtpPacket_matchInstance(pkt, sentCache[i])) {
					return FULL_MATCH;
				} else if ((pkt->origin() == sentCache[i]->origin())&&(pkt->seqno() == sentCache[i]->seqno())) {
					return PARTIAL_MATCH;
				}
			}
			return NO_MATCH;
		}

		void printSentCache() {
			int size = sentCache.size();
			echo("SentCache: ");
			for (int i = 0; i < size; i++) {
				echo("message: %s, origin = %d, thl = %d, seqno = %d",sentCache[i]->payload(),sentCache[i]->origin(),sentCache[i]->thl(),sentCache[i]->seqno());				
			}
		}
	}
	;

}

#endif /* __CTP_FORWARDING_ENGINE_H__ */
