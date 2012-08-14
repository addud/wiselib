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

#define RE_MAX_EVENT_RECEIVERS  2

namespace wiselib {

	template<typename OsModel_P, typename LinkEstimator_P, typename Neigh_P,
		typename RandomNumber_P, typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Debug_P = typename OsModel_P::Debug,
		typename Clock_P = typename OsModel_P::Clock>
	class CtpRoutingEngine : public BasicTopology<OsModel_P, Neigh_P, Radio_P, Timer_P>{
	public:
		typedef OsModel_P OsModel;
		typedef LinkEstimator_P LinkEstimator;
		typedef RandomNumber_P RandomNumber;
		typedef Neigh_P Neighbors;
		typedef Radio_P Radio;
		typedef Timer_P Timer;
		typedef Debug_P Debug;
		typedef Clock_P Clock;

		typedef CtpRoutingEngine<OsModel, LinkEstimator, Neighbors, RandomNumber,
			Radio, Timer, Debug> self_type;
		typedef self_type* self_pointer_t;

		typedef typename Neighbors::mapped_type RoutingTableValue;
		typedef typename Neighbors::iterator RoutingTableIterator;

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
			echo("%d: ", self);
			echo("Re: Destroyed\n");
#endif
		}

		/*
		* Neighbourhood Concept methods 
		*/

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
			echo("%d: ", self);
			echo("Re: Boot for %d\n", self);
#endif

			routeInfoInit(&routeInfo);

			radio().template reg_recv_callback<self_type, &self_type::receive>(
				this);

			le_->template reg_listener_callback<self_type, &self_type::rcv_event>(
				this);

			command_StdControl_start();

			radio().enable_radio();

		}

		// --------------------------------------------------------------------

		void disable(void) {
#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self);
			echo("Re: Disable\n");
#endif

			command_StdControl_stop();
			radio().disable_radio();
		}

		// ----------------------------------------------------------------------------------

		Neighbors &topology() {
			return routingTable;
		}

		// ----------------------------------------------------------------------------------

		template<void (*TMethod)(void)>
		uint8_t reg_listener_callback(void) {

			if (topology_callbacks_.empty())
				topology_callbacks_.assign(RE_MAX_EVENT_RECEIVERS, topology_delegate_t());

			for (TopologyCallbackVectorIterator it = topology_callbacks_.begin();
				it != topology_callbacks_.end(); it++) {
					if ((*it) == event_delegate_t()) {
						(*it) = event_delegate_t::template from_method<TMethod>();
						return 0;
					}
			}

			return -1;
		}


		// --------------------------------------------------------------------

		//int unreg_listener_callback(int idx) {
		//	event_callbacks_.at(idx) = event_delegate_t();
		//	return idx;
		//}

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

		/*
		* CTP Specific methods 
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

		error_t command_CtpInfo_getEtx(ctp_etx_t* etx) {
			if (etx == NULL)
				return ERR_UNSPEC;
			if (routeInfo.parent == INVALID_ADDR)
				return ERR_UNSPEC;
			if (state_is_root) {
				*etx = 0;
			} else {
				// path etx = etx(parent) + etx(link to the parent)
				*etx = routeInfo.etx
					+
					le_->command_LinkEstimator_getLinkQuality(
					routeInfo.parent);
			}
			return SUCCESS;
		}

		// --------------------------------------------------------------------

		void command_CtpInfo_triggerRouteUpdate() {
			updateRouteTask();
			resetInterval();
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

		// --------------------------------------------------------------------

		void command_updateCongestedState(bool congested) {

			congested_state = congested;

			//This can speed up recovery from a congestion situation
			//However, it is not encessarey most of the times
			if (!ECNOff) {
				resetInterval();
			}

		}

		// ----------------------------------------------------------------------------------

		/** sets the current node as a root, if not already a root */
		error_t command_RootControl_setRoot() {
			bool route_found = false;
			route_found = (routeInfo.parent == INVALID_ADDR);
			state_is_root = true;
			routeInfo.parent = radio().id(); //myself
			routeInfo.etx = 0;
			if (route_found) {
				signal_Routing_routeFound();
			}
#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self);
			echo("RootControl.setRoot - I'm a root now! %d\n",
				(int) routeInfo.parent);
#endif
			return SUCCESS;
		}

		// ----------------------------------------------------------------------------------

		error_t command_RootControl_unsetRoot() {
			state_is_root = false;
			routeInfoInit(&routeInfo);
#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self);
			echo("RootControl.unsetRoot - I'm not a root now!\n");
#endif

			updateRouteTask();
			return SUCCESS;
		}

		// ----------------------------------------------------------------------------------

		bool command_RootControl_isRoot() {
			return state_is_root;
		}

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
			ETX_THRESHOLD = 50, // link quality=20% -> ETX=5 -> Metric=50
			PARENT_SWITCH_THRESHOLD = 15,
			MAX_METRIC = RoutingTableValue::MAX_METRIC
		};

		// --------------------------------------------------------------------

		enum TimerPeriods {
			BEACON_INTERVAL = 8192
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

		//Flag to enable/disable the alorithm's reaction to detecting a set Congestion flag in the message header
		//TODO: Pass this as template parameter
		bool ECNOff;
		bool running;
		bool justEvicted;

		RoutingTableValue routeInfo;
		bool state_is_root;

		/* routing table -- routing info about neighbors */
		Neighbors routingTable;

		bool beaconTimerIsSet;
		bool routeTimerIsSet;
		bool emergencyBeaconTimerIsSet;

		uint32_t minInterval;
		uint32_t maxInterval;
		uint32_t currentInterval;
		uint32_t t;
		bool tHasPassed;

		// Node own ID
		node_id_t self;

		bool congested_state;

		EventCallbackVector event_callbacks_;
		TopologyCallbackVector topology_callbacks_;

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

			beaconTimerIsSet = false;
			routeTimerIsSet = false;
			emergencyBeaconTimerIsSet = false;

			//initialize RE parameters with default values
			maxInterval = 512000;
			minInterval = 128;

			ECNOff = false;
			running = false;
			justEvicted = false;

			currentInterval = minInterval;

			state_is_root = false;

			congested_state = false;
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

		// -----------------------------------------------------------------------

		int setTimer(void *userdata, millis_t millis) {

			int timeout = (int) (userdata);

			switch (timeout) {

			case ROUTE_TIMER:

				if (routeTimerIsSet) {
					//echo("routeTimerIsSet");
					return ERR_BUSY;
				}

				routeTimerIsSet = true;
				break;

			case BEACON_TIMER:

				if (beaconTimerIsSet) {
					//echo("beaconTimerIsSet");
					return ERR_BUSY;
				}

				beaconTimerIsSet = true;
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
				if (emergencyBeaconTimerIsSet) {
					//echo("emergencyBeaconTimerIsSet");
					return ERR_BUSY;
				}

				emergencyBeaconTimerIsSet = true;
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
				routeTimerIsSet = false;
				setTimer((void*) ROUTE_TIMER, BEACON_INTERVAL); // because it's a periodic timer.
				event_RouteTimer_fired();
				break;

			case BEACON_TIMER:
				beaconTimerIsSet = false;
				event_BeaconTimer_fired();
				break;

			case EMERGENCY_BEACON_TIMER:
				emergencyBeaconTimerIsSet = false;
				event_BeaconTimer_fired();
				break;

			default:
#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self);
				echo("Re: TimerFiredCallback unexpected timeout: %d\n",
					timeout);
				return;
#endif
				break;
			}
		}

		// ----------------------------------------------------------------------------------

		/* Methods for adjusting the Trickle timeout interval */
		void chooseAdvertiseTime(TimerId timerId) {
			t = currentInterval;
			t /= 2;

			t += random_number().rand(t);
			tHasPassed = false;
			setTimer((void*) timerId, t);
		}

		void resetInterval() {
			currentInterval = minInterval;
			chooseAdvertiseTime(EMERGENCY_BEACON_TIMER);
		}

		void decayInterval() {
			currentInterval *= 2;
			if (currentInterval > maxInterval) {
				currentInterval = maxInterval;
			}
			chooseAdvertiseTime(BEACON_TIMER);
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
				resetInterval();
				setTimer((void*) ROUTE_TIMER, BEACON_INTERVAL);

			} else {
				uint16_t nextInt;

				//set new random routing beacon interval
				nextInt = random_number().randShort(BEACON_INTERVAL);

				nextInt += BEACON_INTERVAL >> 1;
				setTimer((void*) BEACON_TIMER, nextInt);
			}
#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self);
			echo("RE: stdControl.start - running %d\n", running);
#endif
			return SUCCESS;
		}

		// ----------------------------------------------------------------------------------

		error_t command_StdControl_stop() {
			running = false;
#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self);
			echo("RE: stdControl.stop - running %d\n", running);
#endif
			return SUCCESS;
		}

		// ----------------------------------------------------------------------------------

		/* Is this quality measure better than the minimum threshold? */
		// Implemented assuming quality is EETX
		bool passLinkEtxThreshold(ctp_etx_t etx) {
			return true;
			//    	return (etx < ETX_THRESHOLD);
		}

		// ----------------------------------------------------------------------------------

		/* updates the routing information, using the info that has been received
		* from neighbor beacons. Two things can cause this info to change:
		* neighbour beacons, changes in link estimates, including neighbour eviction */
		void updateRouteTask() {
			RoutingTableValue entry;
			RoutingTableIterator it, best;
			ctp_etx_t minEtx;
			ctp_etx_t currentEtx;
			ctp_etx_t linkEtx, pathEtx;

			if (state_is_root)
				return;

			/* Minimum etx found among neighbors, initially infinity */
			minEtx = MAX_METRIC;
			/* Metric through current parent, initially infinity */
			currentEtx = MAX_METRIC;

#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self);
			echo("updateRouteTask");
#endif

			/* Find best path in table, other than our current */
			for (it = routingTable.begin(); it != routingTable.end(); it++) {
				entry = it->second;

				// Avoid bad entries and 1-hop loops
				if (entry.parent == INVALID_ADDR || entry.parent == self) {
#ifdef ROUTING_ENGINE_DEBUG
					echo("%d: ", self);
					echo(
						"routingTable neighbour: [id: %d, neighbour: %d, etx: NO ROUTE]\n",
						(int) it->first, entry.parent);
#endif

					continue;
				}

				/* Compute this neighbor's path metric */
				linkEtx = 
					le_->command_LinkEstimator_getLinkQuality(it->first);

#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self);
				echo(
					"routingTable neighbour: [id: %d, neighbour: %d, etx: %d]\n",
					(int) it->first, entry.parent, (int) linkEtx);
#endif
				pathEtx = linkEtx + entry.etx;

				/* Operations specific to the current parent */
				if (it->first == routeInfo.parent) {
#ifdef ROUTING_ENGINE_DEBUG
					echo("%d: ", self);
					echo("already parent\n");
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
					echo("%d: ", self);
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



			//If minEtx < MAX_METRIC, it means that we have found a better neighbour
			if (minEtx < MAX_METRIC) {

				if (routeInfo.congested) {

					echo("Parent congested, trying to solve: best = %d, parent = %d, bestEtx = %d < %d  ******",best->first,best->second.parent,best->second.etx, routeInfo.etx);
					//printRoutingTable();
				}

				if ((currentEtx == MAX_METRIC)
					|| (routeInfo.congested && (best->second.etx < routeInfo.etx))
					|| (minEtx + PARENT_SWITCH_THRESHOLD < currentEtx)) {
						// routeInfo.metric will not store the composed metric.
						// since the linkMetric may change, we will compose whenever
						// we need it: i. when choosing a parent (here);
						//            ii. when choosing a next hop

#ifdef ROUTING_ENGINE_DEBUG
						echo("%d: ", self);
						echo("Changed parent from %d to %d",
							(int) routeInfo.parent, (int) best->first);
#endif
						le_->command_LinkEstimator_unpinNeighbor(routeInfo.parent);
						le_->command_LinkEstimator_pinNeighbor(best->first);
						le_->command_LinkEstimator_clearDLQ(best->first);

						routeInfo.parent = best->first;
						routeInfo.etx = best->second.etx;
						routeInfo.congested = best->second.congested;

						echo("Updated route: parent = %d, congested = %d, etx = %d",routeInfo.parent,routeInfo.congested, routeInfo.etx);
						//printRoutingTable();
				}
			}

			/* Finally, tell people what happened:  */
			/* We can only loose a route to a parent if it has been evicted. If it hasn't
			* been just evicted then we already did not have a route */
			if (justEvicted && routeInfo.parent == INVALID_ADDR) {
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

			if (!running) {
				return;
			}

			if (from == radio().id()) {
				return;
			}

#ifdef CTP_DEBUGGING
			if (!areConnected(self, from)) {
				return;
			}
#endif

			//Routing beacon => process inside the RE
			RoutingMessage *msg = reinterpret_cast<RoutingMessage*>(data);

			if (msg->msg_id() != CtpRoutingMsgId) {
				return;
			}

			event_BeaconReceive_receive(from, msg);
		}

		void rcv_event(uint8_t event, node_id_t neighbor, block_data_t* msg) {
			switch (event) {
			case (LinkEstimator::LE_EVENT_NEIGHBOUR_EVICTED):
				event_LinkEstimator_evicted(neighbor);
				break;
			case (LinkEstimator::LE_EVENT_SHOULD_INSERT):
				if (event_CompareBit_shouldInsert(msg)) {
					le_->command_LinkEstimator_forceInsertNeighbor(neighbor);
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
			ri->congested = false;
		}

		// ----------------------------------------------------------------------------------

		/* send a beacon advertising this node's routeInfo */
		// only posted if running and radioOn
		void sendBeaconTask() {
			RoutingMessage beaconMsg(CtpRoutingMsgId);

			beaconMsg.set_options(0);

			/* Check congestion state */
			if (congested_state) {
				beaconMsg.set_congestion();
			}
			beaconMsg.set_parent(routeInfo.parent);

			if (state_is_root) {
				beaconMsg.set_etx(routeInfo.etx);
			} else if (routeInfo.parent == INVALID_ADDR) {
				beaconMsg.set_etx(routeInfo.etx);
				beaconMsg.set_pull();
			} else {
				beaconMsg.set_etx(
					routeInfo.etx
					+ 
					le_->command_LinkEstimator_getLinkQuality(
					routeInfo.parent));
			}

#ifdef ROUTING_ENGINE_DEBUG
			echo("%d: ", self);
			echo("sendBeaconTask - parent: %d etx: \n",
				(int) beaconMsg.parent(), (int) beaconMsg.etx());
#endif

			//we need to clone the message, otherwise we get memory errors
			RoutingMessage dup = beaconMsg;

			//echo("beacon sent - parent: %d etx: %d", beaconMsg.parent(), beaconMsg.etx());
			//printRoutingTable();

			radio().send(Radio::BROADCAST_ADDRESS, RoutingMessage::HEADER_SIZE,
				reinterpret_cast<block_data_t*>(&dup));

		}

		// ----------------------------------------------------------------------------------

		void event_RouteTimer_fired() {
			if (running) {
				updateRouteTask();
			}
		}

		// ----------------------------------------------------------------------------------

		void event_BeaconTimer_fired() {
			if (running) {
				if (!tHasPassed) {
					updateRouteTask(); // always the most up to date info
					sendBeaconTask();
#ifdef ROUTING_ENGINE_DEBUG
					echo("%d: ", self);
					echo("Beacon timer fired.\n");
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
		void event_BeaconReceive_receive(node_id_t from,
			RoutingMessage* rcvBeacon) {
				bool congested;

				// we skip the check of beacon length.

				congested = rcvBeacon->congestion();

#ifdef ROUTING_ENGINE_DEBUG
				echo("%d: ", self);
				echo("BeaconReceive.receive - from %d [parent: %d etx: %d] \n",
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
					/*echo("received etx: %d from %d", rcvBeacon->etx(), from);*/
					if (rcvBeacon->etx() == 0) {

#ifdef ROUTING_ENGINE_DEBUG
						echo("%d: ", self);
						echo("from a root, inserting if not in table");
#endif
						le_->command_LinkEstimator_insertNeighbor(from);
						//				echo("Pinned %d", from);
						le_->command_LinkEstimator_pinNeighbor(from);

					}
					routingTableUpdateEntry(from, rcvBeacon->parent(),
						rcvBeacon->etx());

					command_CtpInfo_setNeighborCongested(from, congested);

				}

				if (rcvBeacon->pull()) {
					resetInterval();
				}
				// we do not return routing messages
		}

		// --------------------------------------------------------------------

		void command_CtpInfo_setNeighborCongested(node_id_t n, bool congested) {
			RoutingTableIterator it;
			if ((ECNOff) || (n == INVALID_ADDR))
				return;

			it = routingTable.find(n);
			if (it != routingTable.end()) {
				it->second.congested = congested;
				//echo("---------Setting neighbour %d congested = %d",n, congested);
			}
			if ((routeInfo.congested && !congested) || (routeInfo.parent == n && congested)) {
				updateRouteTask();
			}
		}


		// ----------------------------------------------------------------------------------

		/* It is an event from the LE that signals that a neighbor is no longer reachable. 
		* Need special care if that neighbor is our parent */
		void event_LinkEstimator_evicted(node_id_t neighbor) {
			echo("Neighbour evicted ********************************************");
			routingTableEvict(neighbor);
			if (routeInfo.parent == neighbor) {
				routeInfoInit(&routeInfo);
				justEvicted = true;
				updateRouteTask();
			}
		}

		// -----------------------------------------------------------------------------------

		/* This should see if the node should be inserted in the table.
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
		bool event_CompareBit_shouldInsert(block_data_t *msg) {
			bool found = false;
			ctp_etx_t pathEtx;
			ctp_etx_t neighEtx;
			RoutingTableIterator it;
			RoutingTableValue entry;
			RoutingMessage* rcvBeacon;

			/* 1.determine this packet's path quality */
			rcvBeacon = (RoutingMessage*) msg;

			// checks if it is a RoutingMessage
			if (rcvBeacon == NULL) {
				//TODO: Should forward to the FE??
				echo("%d: CompareBit not Routing message\n", self);
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
					//neighEtx = evaluateEtx(le->LinkEstimator.getLinkQuality(entry->neighbor));
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
			ctp_etx_t etx) {
				RoutingTableIterator it;
				ctp_etx_t linkEtx;
				linkEtx = le_->command_LinkEstimator_getLinkQuality(from);

				it = routingTable.find(from);
				if ((it == routingTable.end())
					&& (routingTable.size() == routingTable.max_size())) {
						//not found and table is full
						//if (passLinkEtxThreshold(linkEtx))
						//TODO: add replacement here, replace the worst
						//}
#ifdef ROUTING_ENGINE_DEBUG
						echo("%d: ", self);
						echo("routingTableUpdateEntry - FAIL, table full\n");
#endif
						return ERR_BUSY;
				} else if (it == routingTable.end()) {
					//not found and there is space
					if (passLinkEtxThreshold(linkEtx)) {
						RoutingTableValue entry = it->second;
						entry.parent = parent;
						entry.etx = etx;
						routingTable[from] = entry;

						/*echo("Added new entry %d",from);
						printRoutingTable();*/

#ifdef ROUTING_ENGINE_DEBUG
						echo("%d: ", self);
						echo("routingTableUpdateEntry - OK, new entry\n");
#endif
					} else {
#ifdef ROUTING_ENGINE_DEBUG
						echo("%d: ", self);
						echo(
							"routingTableUpdateEntry - Fail, link quality (%d) below threshold\n",
							(int) linkEtx);
#endif
					}
				} else {
					//found, just update
					it->first = from;
					it->second.parent = parent;
					it->second.etx = etx;

					//echo("Updated entry %d",from);
					//printRoutingTable();

#ifdef ROUTING_ENGINE_DEBUG
					echo("%d: ", self);
					echo("routingTableUpdateEntry - OK, updated entry\n");
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

		void printRoutingTable() {
			RoutingTableIterator it;
			echo("Routing table:");
			for (it = routingTable.begin(); it != routingTable.end(); it++) {
				echo("neighbour = %d, parent = %d, congested = %d, etx = %d", it->first,it->second.parent,it->second.congested, it->second.etx);
			}
		}

		/*********** end routing table functions ***************/

		// ----------------------------------------------------------------------------------
		// These functions trigger an event in the FE module
		void signal_Routing_routeFound() {
			notify_listeners(RE_EVENT_ROUTE_FOUND);
		}

		// ----------------------------------------------------------------------------------

		void signal_Routing_noRoute() {
			notify_listeners(RE_EVENT_ROUTE_NOT_FOUND);
		}

		// ----------------------------------------------------------------------------------

	}
	;
}

#endif /* __CTP_ROUTING_ENGINE_H__ */
