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

#include "algorithms/topology/basic_topology.h"
#include "algorithms/routing/ctp/ctp_routing_engine_msg.h"
#include "algorithms/routing/ctp/ctp_types.h"
#include "algorithms/routing/ctp/ctp_debugging.h"

namespace wiselib {

/**
 * \brief Routing Engine Module
 *
 * The Routing Engine, an instance of which runs on each node, takes care of sending and receiving routing beacons as well as creating and updating the routing table. This table holds a list of neighbors from which the node can select its parent in the routing tree. The table is filled using the information extracted from the beacons.
 * Along with the identifier of the neighboring nodes, the routing table holds further information, like a metric indicating the “quality” of a node as a potential parent.
 * In the case of CTP, this metric is the ETX (Expected Transmissions), which is communicated by a node to its neighbors through beacons exchange.
 *
 * @ingroup topology_concept
 */

template<typename OsModel_P, typename LinkEstimator_P, typename Neigh_P,
		typename RandomNumber_P, bool ECN_ENABLED,
		typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Debug_P = typename OsModel_P::Debug,
		typename Clock_P = typename OsModel_P::Clock>
class CtpRoutingEngine: public BasicTopology<OsModel_P, Neigh_P, Radio_P,
		Timer_P> {
public:

	//The ECN_ENABLED parameter enables the Explicit Congestion Notification
	//This deals with detecting congestion situations and balancing the traffic accordingly

	static const char RE_MAX_EVENT_RECEIVERS = 2;

	typedef OsModel_P OsModel;
	typedef LinkEstimator_P LinkEstimator;
	typedef RandomNumber_P RandomNumber;
	typedef Neigh_P Neighbors;
	typedef Radio_P Radio;
	typedef Timer_P Timer;
	typedef Debug_P Debug;
	typedef Clock_P Clock;

	typedef CtpRoutingEngine<OsModel, LinkEstimator, Neighbors, RandomNumber,
			ECN_ENABLED, Radio, Timer, Debug> self_type;
	typedef self_type* self_pointer_t;

	typedef typename Neighbors::mapped_type NeighborsValue;
	typedef typename Neighbors::iterator NeighborsIterator;

	typedef typename Radio::node_id_t node_id_t;
	typedef typename Radio::size_t size_t;
	typedef typename Radio::block_data_t block_data_t;
	typedef typename Radio::message_id_t message_id_t;

	typedef typename Timer::millis_t millis_t;
	typedef typename Clock::time_t time_t;

	typedef CtpRoutingEngineMsg<OsModel, Radio> RoutingMessage;

	typedef delegate1<void, uint8_t> event_delegate_t;
	typedef vector_static<OsModel, event_delegate_t, RE_MAX_EVENT_RECEIVERS> EventCallbackVector;
	typedef typename EventCallbackVector::iterator EventCallbackVectorIterator;

	typedef delegate0<void> topology_delegate_t;
	typedef vector_static<OsModel, topology_delegate_t, RE_MAX_EVENT_RECEIVERS> TopologyCallbackVector;
	typedef typename TopologyCallbackVector::iterator TopologyCallbackVectorIterator;

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
		echo("%d: ", self_);
		echo("Re: Destroyed\n");
#endif
	}

    ///@name Topology Concept methods
	///@{

	// --------------------------------------------------------------------
	int init(Radio& radio, Timer& timer, Debug& debug, Clock& clock,
			RandomNumber& random_number, LinkEstimator& le) {
		radio_ = &radio;
		timer_ = &timer;
		debug_ = &debug;
		clock_ = &clock;
		random_number_ = &random_number;
		le_ = &le;

		init_variables();

		return SUCCESS;
	}

	// --------------------------------------------------------------------

	void enable(void) {
#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self_);
		echo("Re: Boot for %d\n", self_);
#endif

		route_info_init(&route_info_);

		radio().template reg_recv_callback<self_type, &self_type::receive>(
				this);

		le_->template reg_listener_callback<self_type, &self_type::rcv_event>(
				this);

		start();

	}

	// --------------------------------------------------------------------

	void disable(void) {
#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self_);
		echo("Re: Disable\n");
#endif

		stop();
	}

	// ----------------------------------------------------------------------------------

	Neighbors &topology() {
		return neighbor_table_;
	}

	// ----------------------------------------------------------------------------------

	template<void (*TMethod)(void)>
	uint8_t reg_listener_callback(void) {

		if (topology_callbacks_.empty())
			topology_callbacks_.assign(RE_MAX_EVENT_RECEIVERS,
					topology_delegate_t());

		for (TopologyCallbackVectorIterator it = topology_callbacks_.begin();
				it != topology_callbacks_.end(); it++) {
			if ((*it) == event_delegate_t()) {
				(*it) = event_delegate_t::template from_method<TMethod>();
				return 0;
			}
		}

		return -1;
	}

	// ----------------------------------------------------------------------------------

	template<class T, void (T::*TMethod)(uint8_t)>
	uint8_t reg_listener_callback(T *obj_pnt) {

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

	int unreg_listener_callback(int idx) {
		event_callbacks_.at(idx) = event_delegate_t();
		return idx;
	}

	///@}

	// -----------------------------------------------------------------------

    ///@name CTP Specific methods
	///@{

	/**
	 *  Simple implementation: return the current routeInfo
	 *  */
	node_id_t get_next_hop() {
		return route_info_.parent;
	}

	// --------------------------------------------------------------------

	/**
	 * Checks if there is a valid route available
	 */
	bool has_route() {
		return (route_info_.parent != INVALID_ADDR);
	}

	// -------------------------------------------------------------------------

	/**
	 * Returns the total ETX value of the current route to the root node
	 */
	error_t get_etx(ctp_etx_t* etx) {
		if (etx == NULL)
			return ERR_UNSPEC;
		if (route_info_.parent == INVALID_ADDR)
			return ERR_UNSPEC;
		if (state_is_root_) {
			*etx = 0;
		} else {
			// path etx = etx(parent) + etx(link to the parent)
			*etx = route_info_.etx + le_->get_link_quality(route_info_.parent);
		}
		return SUCCESS;
	}

	// --------------------------------------------------------------------

	/**
	 * Schedules a route update by resetting the periodic timer interval
	 */
	void schedule_route_update() {
		update_route();
		//print_neighbor_table();
		reset_interval();
	}

	// -----------------------------------------------------------------------

	/**
	 * Triggers an immediate route update based on the current state of the neighbour table
	 */
	void trigger_route_update() {
		update_route();
	}

	// --------------------------------------------------------------------

	/**
	 * Returns the congestion state of the given neighbour
	 */
	bool is_neighbor_congested(node_id_t neighbor) {
		NeighborsIterator it;
		if ((!ECN_ENABLED) || (neighbor == INVALID_ADDR))
			return false;

		it = neighbor_table_.find(neighbor);
		if (it != neighbor_table_.end()) {
			return it->second.congested;
		}
		return false;
	}

	// --------------------------------------------------------------------

	/**
	 * Sets the state of the given neighbour to congested
	 */
	void set_neighbor_congested(node_id_t neighbor, bool congested) {
		NeighborsIterator it;
		if ((!ECN_ENABLED) || (neighbor == INVALID_ADDR))
			return;

		it = neighbor_table_.find(neighbor);
		if (it != neighbor_table_.end()) {
			it->second.congested = congested;
			//echo("---------Setting neighbor %d congested = %d",n, congested);
		}
		if ((route_info_.congested && !congested)
				|| (route_info_.parent == neighbor && congested)) {
			update_route();
		}
	}

	// --------------------------------------------------------------------

	/**
	 * Updates the congested state of the current node
	 */
	void update_congested_state(bool congested) {

		congested_state_ = congested;

		//This can speed up recovery from a congestion situation
		//However, it is not necessary most of the times
		if (ECN_ENABLED) {
			reset_interval();
		}

	}

	// ----------------------------------------------------------------------------------

	/**
	 * Sets the current node as a root, if not already a root
	 * */
	error_t set_root() {
		bool route_found = false;
		route_found = (route_info_.parent == INVALID_ADDR);
		state_is_root_ = true;
		route_info_.parent = radio().id(); //myself
		route_info_.etx = 0;
		if (route_found) {
			signal_route_found();
		}
#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self);
		echo("RootControl.setRoot - I'm a root now! %d\n",
				(int) route_info_.parent);
#endif
		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------

	/**
	 * Removes the root status of the current node
	 */
	error_t unset_root() {
		state_is_root_ = false;
		route_info_init(&route_info_);
#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self);
		echo("RootControl.unsetRoot - I'm not a root now!\n");
#endif

		update_route();
		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------


	/**
	 * Checks if the current node is configured as root
	 */
	bool is_root() {
		return state_is_root_;
	}

	///@}

	// --------------------------------------------------------------------

private:

	// --------------------------------------------------------------------

	enum ErrorCodes {
		SUCCESS = OsModel::SUCCESS,
		ERR_UNSPEC = OsModel::ERR_UNSPEC,
		ERR_NOTIMPL = OsModel::ERR_NOTIMPL,
		ERR_BUSY = OsModel::ERR_BUSY
	};

	// --------------------------------------------------------------------

	enum TreeRouting {
		INVALID_ADDR = Radio::NULL_NODE_ID,
		/// link quality=20% -> ETX=5 -> Metric=50
		ETX_THRESHOLD = 50,
		PARENT_SWITCH_THRESHOLD = 15,
		MAX_METRIC = NeighborsValue::MAX_METRIC
	};

	// --------------------------------------------------------------------

	enum TimerPeriods {
		BEACON_INTERVAL = 8192, MAX_INTERVAL = 512000, MIN_INTERVAL = 128
	};

	// --------------------------------------------------------------------

	enum TimerId {
		BEACON_TIMER = 0, ROUTE_TIMER = 1, EMERGENCY_BEACON_TIMER = 2
	};

	// --------------------------------------------------------------------

	typename Radio::self_pointer_t radio_;
	typename Timer::self_pointer_t timer_;
	typename Debug::self_pointer_t debug_;
	typename Clock::self_pointer_t clock_;
	typename RandomNumber::self_pointer_t random_number_;
	typename LinkEstimator::self_pointer_t le_;

	// --------------------------------------------------------------------

	bool running_;
	bool just_evicted_;

	NeighborsValue route_info_;
	bool state_is_root_;

	/**
	 * Routing Table
	 * The routing table keeps info about neighbor's route_info,
	 * and is used when choosing a parent.
	 * The table is simple:
	 *   - not fragmented
	 *   - not ordered
	 *   - no replacement: eviction follows the LinkEstimator table
	 */
	Neighbors neighbor_table_;

	bool beacon_timer_is_set_;
	bool route_timer_is_set_;
	bool emergency_beacon_timer_is_set_;

	uint32_t current_interval_;
	uint32_t t_;
	bool t_has_passed_;

	/// Node own ID
	node_id_t self_;

	bool congested_state_;

	EventCallbackVector event_callbacks_;
	TopologyCallbackVector topology_callbacks_;

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
		self_ = radio().id();

		//TODO: Pass configuration values as template parameters

		beacon_timer_is_set_ = false;
		route_timer_is_set_ = false;
		emergency_beacon_timer_is_set_ = false;

		running_ = false;
		just_evicted_ = false;

		current_interval_ = MIN_INTERVAL;

		state_is_root_ = false;

		congested_state_ = false;
	}

	// ----------------------------------------------------------------------------------

	error_t start() {
		//start will (re)start the sending of messages
		if (!running_) {
			running_ = true;
			reset_interval();
			set_timer((void*) ROUTE_TIMER, BEACON_INTERVAL);

		} else {
			uint16_t nextInt;

			//set new random routing beacon interval
			nextInt = random_number().short_rand(BEACON_INTERVAL);

			nextInt += BEACON_INTERVAL >> 1;
			set_timer((void*) BEACON_TIMER, nextInt);
		}
#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self);
		echo("RE: stdControl.start - running %d\n", running_);
#endif
		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------

	error_t stop() {
		running_ = false;
#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self_);
		echo("RE: stdControl.stop - running %d\n", running_);
#endif
		return SUCCESS;
	}

	///@}

	// ----------------------------------------------------------------------------------

    ///@name Incoming and Outgoing Event Management
	///@{

	void notify_listeners(uint8_t event) {
		for (EventCallbackVectorIterator it = event_callbacks_.begin();
				it != event_callbacks_.end(); ++it) {
			if (*it != event_delegate_t()) {
				(*it)(event);
			}

		}
	}

	// ----------------------------------------------------------------------------------

	void receive(node_id_t from, size_t len, block_data_t *data) {

		if (!running_) {
			return;
		}

		if (from == radio().id()) {
			return;
		}


		RoutingMessage *msg = reinterpret_cast<RoutingMessage*>(data);

		//Routing beacon => process inside the RE
		if (msg->msg_id() != CtpRoutingMsgId) {
			return;
		}

		/* Handle the receiving of beacon messages from the neighbors. We update the
		 * table, but wait for the next route update to choose a new parent */
		bool congested;

		// we skip the check of beacon length.

		congested = msg->congestion();

#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self);
		echo("BeaconReceive.receive - from %d [parent: %d etx: %d] \n",
				(int) from, (int) (msg->parent()),
				(int) (msg->etx()));
#endif

		//update neighbor table
		if (msg->parent() != INVALID_ADDR) {
			/* If this node is a root, request a forced insert in the link
			 * estimator table and pin the node. */
#ifdef ROUTING_ENGINE_DEBUG
			echo("BeaconReceive.receive - from %d [parent: %d etx: %d]",
					(int) from, (int) (msg->parent()),
					(int) (msg->etx()));
#endif
			/*echo("received etx: %d from %d", msg->etx(), from);*/
			if (msg->etx() == 0) {

#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self_);
				echo("from a root, inserting if not in table");
#endif
				le_->insert_neighbor(from);
				//				echo("Pinned %d", from);
				le_->pin_neighbor(from);

			}
			update_neighbor_entry(from, msg->parent(), msg->etx());

			set_neighbor_congested(from, congested);

		}

		if (msg->pull()) {
			reset_interval();
		}
		// we do not return routing messages
	}

	// ----------------------------------------------------------------------------------

	void rcv_event(uint8_t event, node_id_t neighbor, block_data_t* msg) {
		switch (event) {
		case (LinkEstimator::LE_EVENT_NEIGHBOUR_EVICTED):
			//It is an event from the LE that signals that a neighbor is no longer reachable.
			evict_neighbor(neighbor);
			break;
		case (LinkEstimator::LE_EVENT_SHOULD_INSERT):
			if (should_insert(msg)) {
				le_->force_insert_neighbor(neighbor);
			}
			break;
		default:
			break;
		}
	}

	// ----------------------------------------------------------------------------------

	/**
	 * This function signals the FE module reporting that a route to a root was found
	 */
	void signal_route_found() {
		notify_listeners(RE_EVENT_ROUTE_FOUND);
	}

	// ----------------------------------------------------------------------------------

	/**
	 * This function signals the FE module that the route to the root node has been lost and there is no other available at the moment
	 */
	void signal_no_route() {
		notify_listeners(RE_EVENT_ROUTE_NOT_FOUND);
	}

	///@}

	// -----------------------------------------------------------------------

    ///@name Timer Interface
	///@{

	int set_timer(void *userdata, millis_t millis) {

		int timeout = (int) (userdata);

		switch (timeout) {

		case ROUTE_TIMER:

			if (route_timer_is_set_) {
				//echo("route_timer_is_set_");
				return ERR_BUSY;
			}

			route_timer_is_set_ = true;
			break;

		case BEACON_TIMER:

			if (beacon_timer_is_set_) {
				//echo("beacon_timer_is_set_");
				return ERR_BUSY;
			}

			beacon_timer_is_set_ = true;
			break;

			/*
			 Since the wiselib doesn't allow us to change an already registered timeout callback,
			 and as the number of timer callbacks we can set is limited, we use this separate
			 type of timer to trigger an immmediate beacon send due to special cases, like sudden
			 changes in the network topology.
			 Basically it has the same functionality like the regular BEACON_TIMER,
			 just that it provides a fast-track for special events.
			 */
		case EMERGENCY_BEACON_TIMER:
			if (emergency_beacon_timer_is_set_) {
				//echo("emergency_beacon_timer_is_set_");
				return ERR_BUSY;
			}

			emergency_beacon_timer_is_set_ = true;
			break;
		}

		return timer().template set_timer<self_type, &self_type::timer_elapsed>(
				millis, this, userdata);
	}

	// ----------------------------------------------------------------------------------

	void timer_elapsed(void *userdata) {

		int timeout = (int) (userdata);

		switch (timeout) {

		case ROUTE_TIMER:
			route_timer_is_set_ = false;
			set_timer((void*) ROUTE_TIMER, BEACON_INTERVAL); // because it's a periodic timer.
			route_timer_fired();
			break;

		case BEACON_TIMER:
			beacon_timer_is_set_ = false;
			beacon_timer_fired();
			break;

		case EMERGENCY_BEACON_TIMER:
			emergency_beacon_timer_is_set_ = false;
			beacon_timer_fired();
			break;

		default:
#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self_);
			echo("Re: TimerFiredCallback unexpected timeout: %d\n",
					timeout);
			return;
#endif
			break;
		}
	}

	// ----------------------------------------------------------------------------------

	void route_timer_fired() {
		if (running_) {
			update_route();
		}
	}

	// ----------------------------------------------------------------------------------

	void beacon_timer_fired() {
		if (running_) {
			if (!t_has_passed_) {
				update_route(); // always the most up to date info
				send_beacon();
#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self_);
				echo("Beacon timer fired.\n");
#endif
				remaining_interval();
			} else {
				decay_interval();
			}
		}
	}

	// ----------------------------------------------------------------------------------

	/**
	 *  Method for adjusting the Trickle timeout interval
	 *  */
	void choose_advertise_time(TimerId timerId) {
		t_ = current_interval_;
		t_ /= 2;

		t_ += random_number().rand(t_);
		t_has_passed_ = false;
		set_timer((void*) timerId, t_);
	}

	// ----------------------------------------------------------------------------------

	/**
	 *  Method for adjusting the Trickle timeout interval
	 *  */
	void reset_interval() {
		current_interval_ = MIN_INTERVAL;
		choose_advertise_time(EMERGENCY_BEACON_TIMER);
	}

	// ----------------------------------------------------------------------------------

	/**
	 *  Method for adjusting the Trickle timeout interval
	 *  */
	void decay_interval() {
		current_interval_ *= 2;
		if (current_interval_ > MAX_INTERVAL) {
			current_interval_ = MAX_INTERVAL;
		}
		choose_advertise_time(BEACON_TIMER);
	}

	// ----------------------------------------------------------------------------------

	/**
	 *  Method for adjusting the Trickle timeout interval
	 *  */
	void remaining_interval() {
		uint32_t remaining = current_interval_;
		remaining -= t_;
		t_has_passed_ = true;
		set_timer((void*) BEACON_TIMER, remaining);
	}

	///@}

	// ----------------------------------------------------------------------------------

    ///@name Route Maintenance
	///@{

	/**
	 *  Is this quality measure better than the minimum threshold?
	 *  Implemented assuming quality is EETX
	 *  */
	bool pass_link_etx_threshold(ctp_etx_t etx) {
		return true;
		//    	return (etx < ETX_THRESHOLD);
	}

	// ----------------------------------------------------------------------------------

	/**
	 *  updates the routing information, using the info that has been received from neighbor beacons.
	 *  Two things can cause this info to change:
	 *  neighbor beacons and changes in link estimates, including neighbor eviction
	 *  */
	void update_route() {
		NeighborsValue entry;
		NeighborsIterator it, best;
		ctp_etx_t minEtx;
		ctp_etx_t currentEtx;
		ctp_etx_t linkEtx, pathEtx;

		if (state_is_root_)
			return;

		/* Minimum etx found among neighbors, initially infinity */
		minEtx = MAX_METRIC;
		/* Metric through current parent, initially infinity */
		currentEtx = MAX_METRIC;

#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self);
		echo("update_route");
#endif

		/* Find best path in table, other than our current */
		for (it = neighbor_table_.begin(); it != neighbor_table_.end(); it++) {
			entry = it->second;

			// Avoid bad entries and 1-hop loops
			if (entry.parent == INVALID_ADDR || entry.parent == self_) {
#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self_);
				echo(
						"routingTable neighbor: [id: %d, neighbor: %d, etx: NO ROUTE]\n",
						(int) it->first, entry.parent);
#endif

				continue;
			}

			/* Compute this neighbor's path metric */
			linkEtx = le_->get_link_quality(it->first);

#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self_);
			echo(
					"routingTable neighbor: [id: %d, neighbor: %d, etx: %d]\n",
					(int) it->first, entry.parent, (int) linkEtx);
#endif
			pathEtx = linkEtx + entry.etx;

			/* Operations specific to the current parent */
			if (it->first == route_info_.parent) {
#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self_);
				echo("already parent\n");
#endif
				currentEtx = pathEtx;
				/* update routeInfo with parent's current info */
				route_info_.etx = entry.etx;
				route_info_.congested = entry.congested;
				continue;
			}

			/* Ignore links that are congested */
			if (entry.congested)
				continue;
			/* Ignore links that are bad */
			if (!pass_link_etx_threshold(linkEtx)) {
#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self_);
				echo("did not pass threshold.\n");
#endif
				continue;
			}

			if (pathEtx < minEtx) {
				minEtx = pathEtx;
				best = it;
			}
		}

		/* Now choose between the current parent and the best neighbor */
		/* Requires that:
		 1. at least another neighbor was found with ok quality and not congested
		 2. the current parent is congested and the other best route is at least as good
		 3. or the current parent is not congested and the neighbor quality is better by
		 the PARENT_SWITCH_THRESHOLD.
		 Note: if our parent is congested, in order to avoid forming loops, we try to select
		 a node which is not a descendent of our parent. To ensure that, it needs to have
		 an etx less than our parent's etx, that is routeInfo.ext.
		 */

		//If minEtx < MAX_METRIC, it means that we have found a better neighbor
		if (minEtx < MAX_METRIC) {

			if ((currentEtx == MAX_METRIC)
					|| (route_info_.congested
							&& (best->second.etx < route_info_.etx))
					|| (minEtx + PARENT_SWITCH_THRESHOLD < currentEtx)) {

				// routeInfo.metric will not store the composed metric.
				// since the linkMetric may change, we will compose whenever
				// we need it: i. when choosing a parent (here);
				//            ii. when choosing a next hop

#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self);
				echo("Changed parent from %d to %d",
						(int) route_info_.parent, (int) best->first);
#endif
				le_->unpin_neighbor(route_info_.parent);
				le_->pin_neighbor(best->first);
				le_->clear_DLQ(best->first);

				route_info_.parent = best->first;
				route_info_.etx = best->second.etx;
				route_info_.congested = best->second.congested;

			}
		}

		/* Finally, tell people what happened:  */
		/* We can only loose a route to a parent if it has been evicted. If it hasn't
		 * been just evicted then we already did not have a route */
		if (just_evicted_ && route_info_.parent == INVALID_ADDR) {
			signal_no_route();
		}

		/* On the other hand, if we didn't have a parent (no currentEtx) and now we
		 * do, then we signal route found. The exception is if we just evicted the
		 * parent and immediately found a replacement route: we don't signal in this
		 * case */

		else if (!just_evicted_ && currentEtx == MAX_METRIC
				&& minEtx != MAX_METRIC) {
			signal_route_found();
		}
		just_evicted_ = false;
	}

	// ----------------------------------------------------------------------------------

	/**
	 * Initializes the route structure with default values
	 */
	inline void route_info_init(NeighborsValue *ri) {
		ri->parent = INVALID_ADDR;
		ri->etx = 0;
		ri->congested = false;
	}

	// ----------------------------------------------------------------------------------

	/**
	 *  sends a beacon advertising this node's current route info
	 *  */
	void send_beacon() {
		RoutingMessage beaconMsg(CtpRoutingMsgId);

		beaconMsg.set_options(0);

		/* Check congestion state */
		if (congested_state_) {
			beaconMsg.set_congestion();
		}
		beaconMsg.set_parent(route_info_.parent);

		if (state_is_root_) {
			beaconMsg.set_etx(route_info_.etx);
		} else if (route_info_.parent == INVALID_ADDR) {
			beaconMsg.set_etx(route_info_.etx);
			beaconMsg.set_pull();
		} else {
			beaconMsg.set_etx(
					route_info_.etx
							+ le_->get_link_quality(route_info_.parent));
		}

#ifdef ROUTING_ENGINE_DEBUG
		echo("%d: ", self_);
		echo("send_beacon - parent: %d etx: \n",
				(int) beaconMsg.parent(), (int) beaconMsg.etx());
#endif

		//we need to clone the message, otherwise we get memory errors
		RoutingMessage dup = beaconMsg;

		radio().send(Radio::BROADCAST_ADDRESS, RoutingMessage::HEADER_SIZE,
				reinterpret_cast<block_data_t*>(&dup));

	}

	///@}

	// -----------------------------------------------------------------------------------

	///@name Neighbor Table Interface
	///@{

	/**
	 *  This should see if the node should be inserted in the table.
	 * The link will be recommended for insertion if it is better* than some
	 * link in the routing table that is not our parent.
	 * We are comparing the path quality up to the node, and ignoring the link
	 * quality from us to the node. This is because:
	 *   - we are being optimistic to the nodes in the table, by ignoring the
	 *      1-hop quality to them (which means we are assuming it's 1 as well)
	 *      This actually sets the bar a little higher for replacement
	 *   - this is faster
	 *   - it doesn't require the link estimator to have stabilized on a link
	 */
	bool should_insert(block_data_t *msg) {
		bool found = false;
		ctp_etx_t pathEtx;
		ctp_etx_t neighEtx;
		NeighborsIterator it;
		NeighborsValue entry;
		RoutingMessage* rcvBeacon;

		/* 1.determine this packet's path quality */
		rcvBeacon = (RoutingMessage*) msg;

		// checks if it is a RoutingMessage
		if (rcvBeacon == NULL) {
			//TODO: Should forward to the FE??
			echo("%d: CompareBit not Routing message\n", self_);
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
		for (it = neighbor_table_.begin();
				it != neighbor_table_.end() && !found; it++) {
			entry = it->second;

			//ignore parent, since we can't replace it
			if (it->first == route_info_.parent)
				continue;
			neighEtx = entry.etx;
			//neighEtx = evaluateEtx(le->LinkEstimator.get_linkQuality(entry->neighbor));
			found |= (pathEtx < neighEtx);
		}
		return found;
	}

	// -----------------------------------------------------------------------------------

	error_t update_neighbor_entry(node_id_t from, node_id_t parent,
			ctp_etx_t etx) {
		NeighborsIterator it;
		ctp_etx_t linkEtx;
		linkEtx = le_->get_link_quality(from);

		it = neighbor_table_.find(from);
		if ((it == neighbor_table_.end())
				&& (neighbor_table_.size() == neighbor_table_.max_size())) {
			//not found and table is full
			//if (pass_link_etx_threshold(linkEtx))
			//TODO: add replacement here, replace the worst
			//}
#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self_);
			echo("update_neighbor_entry - FAIL, table full\n");
#endif
			return ERR_BUSY;
		} else if (it == neighbor_table_.end()) {
			//not found and there is space
			if (pass_link_etx_threshold(linkEtx)) {
				NeighborsValue entry = it->second;
				entry.parent = parent;
				entry.etx = etx;
				neighbor_table_[from] = entry;

				//echo("Added new entry %d",from);
				//print_neighbor_table();

#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self_);
				echo("update_neighbor_entry - OK, new entry\n");
#endif
			} else {
#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self_);
				echo(
						"update_neighbor_entry - Fail, link quality (%d) below threshold\n",
						(int) linkEtx);
#endif
			}
		} else {
			//found, just update
			it->first = from;
			it->second.parent = parent;
			it->second.etx = etx;

			//echo("Updated entry %d",from);
			//print_neighbor_table();

#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self);
			echo("update_neighbor_entry - OK, updated entry\n");
#endif
		}

		return SUCCESS;
	}

	// ----------------------------------------------------------------------------------

	/**
	 * Need special care if that neighbor is our parent
	 * */
	void evict_neighbor(node_id_t neighbor) {

		NeighborsIterator it;
		it = neighbor_table_.find(neighbor);

		if (it != neighbor_table_.end()) {

		neighbor_table_.erase(it);
		}

		if (route_info_.parent == neighbor) {
			route_info_init(&route_info_);
			just_evicted_ = true;
			update_route();
		}
	}

	// -----------------------------------------------------------------------

	void print_neighbor_table() {
		NeighborsIterator it;
		echo("Neighbor table:");
		for (it = neighbor_table_.begin(); it != neighbor_table_.end(); it++) {
			echo("neighbor = %d, parent = %d, congested = %d, etx = %d",
					it->first, it->second.parent, it->second.congested,
					it->second.etx);
		}
	}

	///@}

	// ----------------------------------------------------------------------------------

    ///@name Debug Interface
	///@{

	void echo(const char *msg, ...) {
		va_list fmtargs;
		char buffer[1024];
		int i;
		for (i = 0; i < DEBUG_NODES_NR; i++) {
			if (radio().id() == nodes[debug_nodes[i]]) {
				va_start(fmtargs, msg);
				vsnprintf(buffer, sizeof(buffer) - 1, msg, fmtargs);
				va_end(fmtargs);
#ifdef SHAWN
				debug().debug("%d: RE: ", radio().id());
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

#endif /* __CTP_ROUTING_ENGINE_H__ */
