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

#ifndef __CTP_ROUTING_ENGINE_H__
#define __CTP_ROUTING_ENGINE_H__

#include "util/base_classes/routing_base.h"
#include "algorithms/routing/ctp/ctp_routing_engine_msg.h"
#include "algorithms/routing/ctp/ctp_types.h"
#include "algorithms/routing/ctp/ctp_debugging.h"

#define RE_MAX_EVENT_RECEIVERS  2

namespace wiselib {

template<typename OsModel_P, typename RoutingTable_P, typename RandomNumber_P,
		typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Debug_P = typename OsModel_P::Debug,
		typename Clock_P = typename OsModel_P::Clock>
class CtpRoutingEngine: public RoutingBase<OsModel_P, Radio_P> {
public:
	typedef OsModel_P OsModel;
	typedef RandomNumber_P RandomNumber;
	typedef RoutingTable_P RoutingTable;
	typedef Radio_P Radio;
	typedef Timer_P Timer;
	typedef Debug_P Debug;
	typedef Clock_P Clock;

	typedef CtpRoutingEngine<OsModel, RoutingTable, RandomNumber, Radio, Timer,
			Debug> self_type;
	typedef self_type* self_pointer_t;

	typedef typename RoutingTable::mapped_type RoutingTableValue;
	typedef typename RoutingTable::iterator RoutingTableIterator;

	typedef typename Radio::node_id_t node_id_t;
	typedef typename Radio::size_t size_t;
	typedef typename Radio::block_data_t block_data_t;
	typedef typename Radio::message_id_t message_id_t;

	typedef typename Timer::millis_t millis_t;
	typedef typename Clock::time_t time_t;
	typedef typename RandomNumber::value_t value_t;

	typedef CtpRoutingEngineMsg<OsModel, Radio> RoutingMessage;

	typedef delegate3<void, node_id_t, size_t, block_data_t*> radio_delegate_t;
	typedef vector_static<OsModel, radio_delegate_t, RADIO_BASE_MAX_RECEIVERS> RecvCallbackVector;
	typedef typename RecvCallbackVector::iterator RecvCallbackVectorIterator;

	typedef delegate1<void, uint8_t> event_delegate_t;
	typedef vector_static<OsModel, event_delegate_t, RE_MAX_EVENT_RECEIVERS> EventCallbackVector;
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
		MAX_MESSAGE_LENGTH = Radio_P::MAX_MESSAGE_LENGTH - sizeof(node_id_t), ///< Maximum message length for the radio - one address space
		RANDOM_MAX = RandomNumber::RANDOM_MAX ///< Maximum random number that can be generated
	};

	// --------------------------------------------------------------------

	enum TreeRouting {
		BEACON_INTERVAL = 8192, INVALID_ADDR = NULL_NODE_ID, ETX_THRESHOLD = 50, // link quality=20% -> ETX=5 -> Metric=50
		PARENT_SWITCH_THRESHOLD = 15,
		MAX_METRIC = RoutingTableValue::MAX_METRIC
	};

	// --------------------------------------------------------------------

	enum Events {
		RE_EVENT_ROUTE_NOT_FOUND = 0, RE_EVENT_ROUTE_FOUND = 1
	};

	// --------------------------------------------------------------------

	CtpRoutingEngine() {
	}

	// --------------------------------------------------------------------

	~CtpRoutingEngine() {
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
			RandomNumber& random_number) {
		radio_ = &radio;
		timer_ = &timer;
		debug_ = &debug;
		clock_ = &clock;
		random_number_ = &random_number;

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
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("Re: Boot for %d\n", self);
#endif

		routeInfoInit(&routeInfo);

		// Call the corresponding rootcontrol command
		isRoot ?
				command_RootControl_setRoot() : command_RootControl_unsetRoot();

		random_number().srand(
				clock().milliseconds(clock().time()) * (3 * self + 2));
		command_StdControl_start();

		radio().template reg_recv_callback<self_type, &self_type::receive>(
				this);

		radio().template reg_event_callback<self_type, &self_type::rcv_event>(
				this);

		timer().template set_timer<self_type, &self_type::timer_elapsed>(15000,
				this, 0);

		return radio().enable_radio();

	}

	// --------------------------------------------------------------------

	int disable_radio(void) {
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("Re: Disable\n");
#endif

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

		/*
		 * Insert the destination address at the beginning of the message
		 * This helps differentiating between received routing beacons and data messages
		 */
		//TODO: Warning: variable length array
		block_data_t buffer[len + sizeof(node_id_t)];
		write<OsModel, block_data_t, node_id_t>(buffer, destination);
		memcpy(buffer + sizeof(node_id_t), data, len);
		len += sizeof(node_id_t);
		return radio().send(destination, len, buffer);
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

	/*
	 * UnicastNameFreeRouting Inteface -----------------------------------------
	 */
	/* Simple implementation: return the current routeInfo */
	node_id_t command_Routing_nextHop() {
		return routeInfo.parent;
	}

	// --------------------------------------------------------------------

	bool command_Routing_hasRoute() {
		return (routeInfo.parent != INVALID_ADDR);
	}

	// -------------------------------------------------------------------------

	error_t command_CtpInfo_getEtx(ctp_msg_etx_t* etx) {
		echo("6");
		if (etx == NULL)
			return ERR_UNSPEC;
		if (routeInfo.parent == INVALID_ADDR)
			return ERR_UNSPEC;
		if (state_is_root) {
			*etx = 0;
		} else {
			// path etx = etx(parent) + etx(link to the parent)
			echo("7");
			*etx = routeInfo.etx
					+ evaluateEtx(
							radio().command_LinkEstimator_getLinkQuality(
									routeInfo.parent));
			echo("8");
		}
		echo("9");
		return SUCCESS;
	}

	// --------------------------------------------------------------------

	void command_CtpInfo_recomputeRoutes() {
		post_updateRouteTask();
	}

	// --------------------------------------------------------------------

	void command_CtpInfo_triggerRouteUpdate() {
		resetInterval();
	}

	// --------------------------------------------------------------------

	void command_CtpInfo_triggerImmediateRouteUpdate() {
		resetInterval();
	}

	// --------------------------------------------------------------------

	void command_CtpInfo_setNeighborCongested(node_id_t n, bool congested) {
		RoutingTableIterator it;
		if ((ECNOff) || (n == INVALID_ADDR))
			return;

		it = routingTable.find(n);
		if (it != routingTable.end()) {
			it->second.congested = congested;
		}
		if (routeInfo.congested && !congested)
			post_updateRouteTask();
		else if (routeInfo.parent == n && congested)
			post_updateRouteTask();
	}

	// --------------------------------------------------------------------

	bool command_CtpInfo_isNeighborCongested(node_id_t n) {
		RoutingTableIterator it;
		if ((ECNOff) || (n == INVALID_ADDR))
			return false;

		it = routingTable.find(n);
		if (it != routingTable.end()) {
			return it->second.congested;
		}
		return false;
	}

	// ----------------------------------------------------------------------------------

	/** sets the current node as a root, if not already a root */
	/*  returns FAIL if it's not possible for some reason      */
	error_t command_RootControl_setRoot() {
		bool route_found = false;
		route_found = (routeInfo.parent == INVALID_ADDR);
		state_is_root = true;
		routeInfo.parent = self; //myself
		routeInfo.etx = 0;
		if (route_found) {
			signal_Routing_routeFound();
		}
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("RootControl.setRoot - I'm a root now! %d\n",
				(int) routeInfo.parent);
#endif
		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------

	error_t command_RootControl_unsetRoot() {
		state_is_root = false;
		routeInfoInit(&routeInfo);
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("RootControl.unsetRoot - I'm not a root now!\n");
#endif

		post_updateRouteTask();
		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------

	bool command_RootControl_isRoot() {
		return state_is_root;
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

	//Flag to enable/disable the alorithm's reaction to detecting a set Congestion flag in the message header
	//TODO: Pass this as template parameter
	bool ECNOff;
	bool radioOn;
	bool running;
	bool justEvicted;

	RoutingTableValue routeInfo;
	bool state_is_root;

	RoutingMessage beaconMsg;

	/* routing table -- routing info about neighbors */
	RoutingTable routingTable;

	uint32_t routeUpdateTimerCount;

	uint32_t minInterval;
	uint32_t maxInterval;
	uint32_t currentInterval;
	uint32_t t;
	bool tHasPassed;

	// Node own ID
	node_id_t self;

	// Sets a node as root
	bool isRoot;

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

		//TODO: Pass configuration values as template parameters

		//initialize RE parameters with default values
		maxInterval = 512000;
		minInterval = 128;
		isRoot = false;

		ECNOff = true;
		radioOn = true; // TO IMPLEMENT ------ radioOn in stdcontrol
		running = false;
		justEvicted = false;

		currentInterval = minInterval;

		state_is_root = false;
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
				debug().debug("%d: RE: ", id());
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

		case ROUTE_TIMER: {
			setTimer((void*) ROUTE_TIMER, BEACON_INTERVAL); // because it's a periodic timer.
			event_RouteTimer_fired();
			break;
		}
		case BEACON_TIMER: {
			event_BeaconTimer_fired();
			break;
		}
		case POST_UPDATEROUTETASK: {
			updateRouteTask();
			break;
		}

		case POST_SENDBEACONTASK: {
			sendBeaconTask();
			break;
		}

		default: {
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug("%d: ", self);
			debug().debug("Re: TimerFiredCallback unexpected timeout: %d\n",
					timeout);
			return;
#endif
			break;
		}
		}
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("Re: TimerFiredCallback, trigger: %s.\n",
				this->timeout_period_message_[timeout]);
#endif
	}

	// ----------------------------------------------------------------------------------

	/* Methods for adjusting the Tricckle timeout interval */
	void chooseAdvertiseTime() {
		t = currentInterval;
		t /= 2;

		/* TODO: Need to clarify the use of the Random Wiselib concept */

		t += random_number().rand(t);
		tHasPassed = false;
		setTimer((void*) BEACON_TIMER, t);
	}

	void resetInterval() {
		currentInterval = minInterval;
		chooseAdvertiseTime();
	}

	void decayInterval() {
		currentInterval *= 2;
		if (currentInterval > maxInterval) {
			currentInterval = maxInterval;
		}
		chooseAdvertiseTime();
	}

	void remainingInterval() {
		uint32_t remaining = currentInterval;
		remaining -= t;
		tHasPassed = true;
		setTimer((void*) BEACON_TIMER, remaining);
	}

	// ----------------------------------------------------------------------------------

	error_t command_StdControl_start() {
		//start will (re)start the sending of messages
		if (!running) {
			running = true;
			radioOn = true;
			resetInterval();
			setTimer((void*) ROUTE_TIMER, BEACON_INTERVAL);

			//set new random routing beacon interval
			if (running) {
				uint16_t nextInt;

				//TODO: Investigate random number generation
				nextInt = random_number().rand(BEACON_INTERVAL);
				//nextInt = command_Random_rand16(0) % BEACON_INTERVAL ;

				nextInt += BEACON_INTERVAL >> 1;
				setTimer((void*) BEACON_TIMER, nextInt);
			}
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug("%d: ", self);
			debug().debug("RE: stdControl.start - running %d\n", running);
#endif
		}
		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------

	error_t command_StdControl_stop() {
		running = false;
		radioOn = false;
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("RE: stdControl.stop - running %d\n", running);
#endif
		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------

	/* Is this quality measure better than the minimum threshold? */
	// Implemented assuming quality is EETX
	bool passLinkEtxThreshold(uint16_t etx) {
		return true;
		//    	return (etx < ETX_THRESHOLD);
	}

	// ----------------------------------------------------------------------------------

	/* Converts the output of the link estimator to path metric
	 * units, that can be *added* to form path metric measures */
	uint16_t evaluateEtx(uint16_t quality) {
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("evaluateEtx - %d -> %d\n", (int) quality,
				(int) (quality + 10));
#endif
		//TODO: Removed this while using the number of hops metric
		return (quality + 10);
//		return quality;
	}

	// ----------------------------------------------------------------------------------

	/* updates the routing information, using the info that has been received
	 * from neighbor beacons. Two things can cause this info to change:
	 * neighbor beacons, changes in link estimates, including neighbor eviction */
	void updateRouteTask() {
		RoutingTableValue entry;
		RoutingTableIterator it, best;
		bool best_found = false;
		uint16_t minEtx;
		uint16_t currentEtx;
		uint16_t linkEtx, pathEtx;

		if (state_is_root)
			return;

		/* Minimum etx found among neighbors, initially infinity */
		minEtx = MAX_METRIC;
		/* Metric through current parent, initially infinity */
		currentEtx = MAX_METRIC;

#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("updateRouteTask");
#endif

		/* Find best path in table, other than our current */
		for (it = routingTable.begin(); it != routingTable.end(); it++) {
			entry = it->second;

			// Avoid bad entries and 1-hop loops
			if (entry.parent == INVALID_ADDR || entry.parent == self) {
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug("%d: ", self);
				debug().debug(
						"routingTable neighbour: [id: %d, neighbour: %d, etx: NO ROUTE]\n",
						(int) it->first, entry.parent);
#endif

				continue;
			}

			/* Compute this neighbor's path metric */
			linkEtx = evaluateEtx(
					radio().command_LinkEstimator_getLinkQuality(it->first));

#ifdef ROUTING_ENGINE_DEBUG
			debug().debug("%d: ", self);
			debug().debug(
					"routingTable neighbour: [id: %d, neighbour: %d, etx: %d]\n",
					(int) it->first, entry.parent, (int) linkEtx);
#endif
			pathEtx = linkEtx + entry.etx;

			/* Operations specific to the current parent */
			if (it->first == routeInfo.parent) {
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug("%d: ", self);
				debug().debug("already parent\n");
#endif
				currentEtx = pathEtx;
				/* update routeInfo with parent's current info */
				routeInfo.etx = entry.etx;
				routeInfo.congested = entry.congested;
				continue;
			}

			/* Ignore links that are congested */
			if (entry.congested)
				continue;
			/* Ignore links that are bad */
			if (!passLinkEtxThreshold(linkEtx)) {
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug("%d: ", self);
				debug().debug("did not pass threshold.\n");
#endif
				continue;
			}

			if (pathEtx < minEtx) {
				minEtx = pathEtx;
				best = it;
				best_found = true;
			}
		}

		/* Now choose between the current parent and the best neighbor */
		/* Requires that:
		 1. at least another neighbor was found with ok quality and not congested
		 2. the current parent is congested and the other best route is at least as good
		 3. or the current parent is not congested and the neighbor quality is better by
		 the PARENT_SWITCH_THRESHOLD.
		 Note: if our parent is congested, in order to avoid forming loops, we try to select
		 a node which is not a descendent of our parent. routeInfo.ext is our parent's
		 etx. Any descendent will be at least that + 10 (1 hop), so we restrict the
		 selection to be less than that.
		 */

		if (minEtx != MAX_METRIC) {
			if (currentEtx == MAX_METRIC
					|| (routeInfo.congested && (minEtx < (routeInfo.etx + 10)))
					|| minEtx + PARENT_SWITCH_THRESHOLD < currentEtx) {
				// routeInfo.metric will not store the composed metric.
				// since the linkMetric may change, we will compose whenever
				// we need it: i. when choosing a parent (here);
				//            ii. when choosing a next hop

				if (best_found) {

#ifdef ROUTING_ENGINE_DEBUG
					debug().debug("%d: ", self);
					debug().debug("Changed parent from %d to %d",
							(int) routeInfo.parent, (int) best->first);
#endif
					radio().command_LinkEstimator_unpinNeighbor(
							routeInfo.parent);
					radio().command_LinkEstimator_pinNeighbor(best->first);
					radio().command_LinkEstimator_clearDLQ(best->first);

					routeInfo.parent = best->first;
					routeInfo.etx = best->second.etx;
					routeInfo.congested = best->second.congested;
				} else {
					debug().debug(
							"%d: EROR. Trying to use a NULL best routing node.",
							self);
				}
			}
		}

		/* Finally, tell people what happened:  */
		/* We can only loose a route to a parent if it has been evicted. If it hasn't
		 * been just evicted then we already did not have a route */
		if (justEvicted && routeInfo.parent == INVALID_ADDR) {
			//TODO Callback to FE to signal no route found
			signal_Routing_noRoute();
		}
		/* On the other hand, if we didn't have a parent (no currentEtx) and now we
		 * do, then we signal route found. The exception is if we just evicted the
		 * parent and immediately found a replacement route: we don't signal in this
		 * case */
		else if (!justEvicted && currentEtx == MAX_METRIC
				&& minEtx != MAX_METRIC) {
			signal_Routing_routeFound();
		}
		justEvicted = false;
	}

	// ----------------------------------------------------------------------------------

	void receive(node_id_t from, size_t len, block_data_t *data) {
		if (from == radio().id()) {
			return;
		}

//#ifdef CTP_DEBUGGING
//		if (!areConnected(self, from)) {
//			return;
//		}
//#endif

		//Read and remove the destination from the beginning of the message
		node_id_t destination = read<OsModel, block_data_t, node_id_t>(data);
		data += sizeof(node_id_t);
		len -= sizeof(node_id_t);

		if (destination != BROADCAST_ADDRESS) {
			//Data message => forward to the FE
			echo("to FE length %d",len);
			notify_receivers(from, len, data);
		} else {
			//Routing beacon => process inside the RE
			RoutingMessage *message = reinterpret_cast<RoutingMessage*>(data);
			event_BeaconReceive_receive(from, message);
		}
	}

	void rcv_event(uint8_t event, node_id_t neighbor, block_data_t* msg) {
		switch (event) {
		case (Radio_P::LE_EVENT_NEIGHBOUR_EVICTED):
			event_LinkEstimator_evicted(neighbor);
			break;
		case (Radio::LE_EVENT_SHOULD_INSERT):
			if (event_CompareBit_shouldInsert(msg)) {
				radio().command_LinkEstimator_forceInsertNeighbor(neighbor);
			}
			break;
		default:
			break;
		}
	}

// ----------------------------------------------------------------------------------

	inline void routeInfoInit(RoutingTableValue *ri) {
		ri->parent = INVALID_ADDR;
		ri->etx = 0;
		ri->haveHeard = 0;
		ri->congested = false;
	}

// ----------------------------------------------------------------------------------

	/* send a beacon advertising this node's routeInfo */
// only posted if running and radioOn
	void sendBeaconTask() {
		error_t eval;

		beaconMsg.set_options(0);

		/* Congestion notification: am I congested? */
		//TODO: Needs to be done from the FE
		//		if (cfe->command_CtpCongestion_isCongested()) {
		//			beaconMsg.set_congestion();
		//		}
		beaconMsg.set_parent(routeInfo.parent);
		if (state_is_root) {
			beaconMsg.set_etx(routeInfo.etx);
		} else if (routeInfo.parent == INVALID_ADDR) {
			beaconMsg.set_etx(routeInfo.etx);
			beaconMsg.set_pull();
		} else {
			beaconMsg.set_etx(
					routeInfo.etx
							+ evaluateEtx(
									radio().command_LinkEstimator_getLinkQuality(
											routeInfo.parent)));
		}

#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("sendBeaconTask - parent: %d etx: \n",
				(int) beaconMsg.parent(), (int) beaconMsg.etx());
#endif

		eval = send(BROADCAST_ADDRESS, beaconMsg.buffer_size(),
				(block_data_t*) &beaconMsg); // the duplicate will be deleted in the LE module, we keep a copy here that is reused each time.

		if (eval == SUCCESS) {
//				echo("beacon sent - parent: %d etx: %d", (int) beaconMsg.parent(),
//					(int) beaconMsg.etx());
		} else {
			radioOn = false;
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug("%d: ", self);
			debug().debug("sendBeaconTask - running: %d, radioOn: %d \n",
					running, radioOn);
#endif
		}
	}

// ----------------------------------------------------------------------------------

	void event_RouteTimer_fired() {
		if (radioOn && running) {
			post_updateRouteTask();
		}
	}

// ----------------------------------------------------------------------------------

	void event_BeaconTimer_fired() {
		if (radioOn && running) {
			if (!tHasPassed) {
				post_updateRouteTask(); // always the most up to date info
				post_sendBeaconTask();
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug("%d: ", self);
				debug().debug("Beacon timer fired.\n");
#endif
				remainingInterval();
			} else {
				decayInterval();
			}
		}
	}

// ----------------------------------------------------------------------------------

	/* Handle the receiving of beacon messages from the neighbors. We update the
	 * table, but wait for the next route update to choose a new parent */
	void event_BeaconReceive_receive(node_id_t from, RoutingMessage* msg) {
		bool congested;
		RoutingMessage* rcvBeacon;

		rcvBeacon = msg;
		// we skip the check of beacon length.

		// 		CtpBeacon* rcvBeacon = check_and_cast<CtpBeacon*>(msg) ;

		congested = rcvBeacon->congestion();

#ifdef ROUTING_ENGINE_DEBUG
		debug().debug("%d: ", self);
		debug().debug("BeaconReceive.receive - from %d [parent: %d etx: %d] \n",
				(int) from, (int) (rcvBeacon->parent()),
				(int) (rcvBeacon->etx()));
#endif

		//update neighbor table
		if (rcvBeacon->parent() != INVALID_ADDR) {
			/* If this node is a root, request a forced insert in the link
			 * estimator table and pin the node. */
#ifdef ROUTING_ENGINE_DEBUG
			echo("BeaconReceive.receive - from %d [parent: %d etx: %d]",
					(int) from, (int) (rcvBeacon->parent()),
					(int) (rcvBeacon->etx()));
#endif
//			echo("received etx: %d from %d", rcvBeacon->etx(), from);
			if (rcvBeacon->etx() == 0) {

#ifdef ROUTING_ENGINE_DEBUG
				debug().debug("%d: ", self);
				debug().debug("from a root, inserting if not in table");
#endif
				radio().command_LinkEstimator_insertNeighbor(from);
//				echo("Pinned %d", from);
				radio().command_LinkEstimator_pinNeighbor(from);

			}
//TODO: also, if better than my current parent's path etx, insert
			routingTableUpdateEntry(from, rcvBeacon->parent(),
					rcvBeacon->etx());

			command_CtpInfo_setNeighborCongested(from, congested);

		}

		if (rcvBeacon->pull()) {
			resetInterval();
		}
		// we do not return routing messages
	}

// ----------------------------------------------------------------------------------

	/* Signals that a neighbor is no longer reachable. need special care if
	 * that neighbor is our parent */
	void event_LinkEstimator_evicted(node_id_t neighbor) {
		routingTableEvict(neighbor);
		if (routeInfo.parent == neighbor) {
			routeInfoInit(&routeInfo);
			justEvicted = true;
			post_updateRouteTask();
		}
	}

// -----------------------------------------------------------------------------------

// default events Routing.noRoute and Routing.routeFound skipped -> useless

	/* This should see if the node should be inserted in the table.
	 * If the white_bit is set, this means the LE believes this is a good
	 * first hop link.
	 * The link will be recommended for insertion if it is better* than some
	 * link in the routing table that is not our parent.
	 * We are comparing the path quality up to the node, and ignoring the link
	 * quality from us to the node. This is because:
	 *   1. because of the white bit, we assume that the 1-hop to the candidate
	 *      link is good (say, etx=1)
	 *   2. we are being optimistic to the nodes in the table, by ignoring the
	 *      1-hop quality to them (which means we are assuming it's 1 as well)
	 *      This actually sets the bar a little higher for replacement
	 *   3. this is faster
	 *   4. it doesn't require the link estimator to have stabilized on a link
	 */
	bool event_CompareBit_shouldInsert(block_data_t *msg) {
		bool found = false;
		uint16_t pathEtx;
		uint16_t neighEtx;
		RoutingTableIterator it;
		RoutingTableValue entry;
		RoutingMessage* rcvBeacon;

		/* 1.determine this packet's path quality */
		rcvBeacon = (RoutingMessage*) msg;

		// checks if it is a RoutingMessage
		if (rcvBeacon == NULL) {
			//TODO: Should forward to the FE??
			debug().debug("%d: CompareBit not Routing message\n", self);
			return false;
		}

		if (rcvBeacon->parent() == INVALID_ADDR) {
			return false;
		}

		/* the node is a root, recommend insertion! */
		if (rcvBeacon->etx() == 0) {
			return true;
		}

		pathEtx = rcvBeacon->etx();

		/* 2. see if we find some neighbor that is worse */
		for (it = routingTable.begin(); it != routingTable.end() && !found;
				it++) {
			entry = it->second;

			//ignore parent, since we can't replace it
			if (it->first == routeInfo.parent)
				continue;
			neighEtx = entry.etx;
			//neighEtx = evaluateEtx(radio().LinkEstimator.getLinkQuality(entry->neighbor));
			found |= (pathEtx < neighEtx);
		}
		return found;
	}

	/************************************************************/
	/* Routing Table Functions                                  */

	/* The routing table keeps info about neighbor's route_info,
	 * and is used when choosing a parent.
	 * The table is simple:
	 *   - not fragmented
	 *   - not ordered
	 *   - no replacement: eviction follows the LinkEstimator table
	 ************************************************************/

	error_t routingTableUpdateEntry(node_id_t from, node_id_t parent,
			uint16_t etx) {
		RoutingTableIterator it;
		uint16_t linkEtx;
		linkEtx = evaluateEtx(
				radio().command_LinkEstimator_getLinkQuality(from));

		it = routingTable.find(from);
		if ((it == routingTable.end())
				&& (routingTable.size() == routingTable.max_size())) {
//not found and table is full
//if (passLinkEtxThreshold(linkEtx))
//TODO: add replacement here, replace the worst
//}
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug("%d: ", self);
			debug().debug("routingTableUpdateEntry - FAIL, table full\n");
#endif
			return ERR_BUSY;
		} else if (it == routingTable.end()) {
//not found and there is space
			if (passLinkEtxThreshold(linkEtx)) {
				RoutingTableValue entry = it->second;
				entry.parent = parent;
				entry.etx = etx;
				entry.haveHeard = 1;
				entry.congested = false;
				routingTable[from] = entry;

#ifdef ROUTING_ENGINE_DEBUG
				debug().debug("%d: ", self);
				debug().debug("routingTableUpdateEntry - OK, new entry\n");
#endif
			} else {
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug("%d: ", self);
				debug().debug(
						"routingTableUpdateEntry - Fail, link quality (%d) below threshold\n",
						(int) linkEtx);
#endif
			}
		} else {
//found, just update
			it->first = from;
			it->second.parent = parent;
			it->second.etx = etx;
			it->second.haveHeard = 1;
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug("%d: ", self);
			debug().debug("routingTableUpdateEntry - OK, updated entry\n");
#endif
		}

		return SUCCESS;
	}

// ----------------------------------------------------------------------------------

	error_t routingTableEvict(node_id_t neighbor) {
		RoutingTableIterator it;
		it = routingTable.find(neighbor);

		if (it == routingTable.end()) {
			return ERR_UNSPEC;
		}

		routingTable.erase(it);
		return SUCCESS;
	}

	/*********** end routing table functions ***************/

// ----------------------------------------------------------------------------------
// These functions trigger an event in the module where they should signal it.
	void signal_Routing_routeFound() {
		//TODO: signal route found to FE
		//cfe->event_UnicastNameFreeRouting_routeFound() ;
		notify_listeners(RE_EVENT_ROUTE_FOUND);
	}
	void signal_Routing_noRoute() {
		//TODO: signal route not found to FE
		//cfe->event_UnicastNameFreeRouting_routeFound() ;
		notify_listeners(RE_EVENT_ROUTE_NOT_FOUND);
	}

// ----------------------------------------------------------------------------------

// these functions simulate the post command of TinyOs
	void post_updateRouteTask() {
		setTimer((void*) POST_UPDATEROUTETASK, 0); // cannot call the updateRouteTask directly. By this way it is more similar to the post command in TinyOs.
	}

	void post_sendBeaconTask() {
		setTimer((void*) POST_SENDBEACONTASK, 0);
	}

// ----------------------------------------------------------------------------------
}
;

// ----------------------------------------------------------------------------------

template<typename OsModel_P, typename RoutingTable_P, typename RandomNumber_P,
		typename Radio_P, typename Timer_P, typename Debug_P, typename Clock_P> const char * CtpRoutingEngine<
		OsModel_P, RoutingTable_P, RandomNumber_P, Radio_P, Timer_P, Debug_P,
		Clock_P>::timeout_period_message_[] = { "BEACON_TIMER", "ROUTE_TIMER",
		"POST_UPDATEROUTETASK", "POST_SENDBEACONTASK" };

// ----------------------------------------------------------------------------------

}

#endif /* __CTP_ROUTING_ENGINE_H__ */
