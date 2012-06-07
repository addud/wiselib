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
#ifndef __CTP_ROUTING_ENGINE_H__
#define __CTP_ROUTING_ENGINE_H__

#include "util/base_classes/routing_base.h"
#include "config.h"
#include <string.h>
#include "algorithms/routing/ctp/ctp_routing_engine_msg.h"
#include "algorithms/routing/ctp/ctp_link_estimator.h"
#include "algorithms/routing/ctp/ctp_types.h"
#include "algorithms/routing/ctp/ctp_random_number.h"

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
//	typedef typename wiselib::CtpRandomNumber<OsModel> RandomNumber;
	typedef RoutingTable_P RoutingTable;
	typedef Radio_P Radio;
	typedef Timer_P Timer;
	typedef Debug_P Debug;
	typedef Clock_P Clock;

	typedef CtpRoutingEngine<OsModel, RoutingTable, RandomNumber, Radio, Timer,
			Debug> self_type;
	typedef self_type* self_pointer_t;

	typedef typename Radio::node_id_t node_id_t;
	typedef typename Radio::size_t size_t;
	typedef typename Radio::block_data_t block_data_t;
	typedef typename Radio::message_id_t message_id_t;

	typedef typename Timer::millis_t millis_t;
	typedef typename Clock::time_t time_t;
	typedef typename RandomNumber::value_t value_t;

	typedef CtpRoutingEngineMsg<OsModel, Radio> RoutingMessage;
	typedef CtpLinkEstimator<OsModel, RandomNumber, Radio, Timer, Debug, Clock> LinkEstimator;
//	typedef CtpLinkEstimator<OsModel, RoutingTable> LinkEstimator;
//	typedef wiselib::vector_static<Os, Os::Radio::node_id_t, 10> node_vector_t;
//	typedef wiselib::StaticArrayRoutingTable<Os, Os::Radio, 54, wiselib::DsrRoutingTableValue<Os::Radio, node_vector_t> > routing_table_t;
//	typedef wiselib::DsrRouting<Os, routing_table_t> routing_t;

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
		MAX_MESSAGE_LENGTH = Radio_P::MAX_MESSAGE_LENGTH, ///< Maximal number of bytes in payload
		RANDOM_MAX = RandomNumber::RANDOM_MAX ///< Maximum random number that can be generated
	};
	// --------------------------------------------------------------------

	enum TimeoutPeriods {
		BEACON_TIMER = 1,
		ROUTE_TIMER = 2,
		POST_UPDATEROUTETASK = 3,
		POST_SENDBEACONTASK = 4
	};

	// --------------------------------------------------------------------
	//Timeout Periods
//	static const int BEACON_TIMER = 1;
//	static  const int ROUTE_TIMER = 2;
//	static const int POST_UPDATEROUTETASK = 3;
//	static const int POST_SENDBEACONTASK = 4;

	enum TreeRouting {
		AM_TREE_ROUTING_CONTROL = 0xCE,
		BEACON_INTERVAL = 8192,
		INVALID_ADDR = NULL_NODE_ID,
		ETX_THRESHOLD = 50, // link quality=20% -> ETX=5 -> Metric=50
		PARENT_SWITCH_THRESHOLD = 15,
		MAX_METRIC = 0xFFFF
	};

	typedef struct

	{
		node_id_t parent;
		ctp_msg_etx_t etx;
		bool haveHeard;
		bool congested;
	} route_info_t;

	typedef struct {
		node_id_t neighbor;
		route_info_t info;
	} routing_table_entry;

	LinkEstimator le;

	/////////////////////// CtpRoutingEngineP.nc //////////////////////////

	bool ECNOff;
	bool radioOn;
	bool running;
	bool sending;
	bool justEvicted;

	route_info_t routeInfo;
	bool state_is_root;
	node_id_t my_ll_addr;

	//	cPacket beaconMsgBuffer;
	//	CtpBeacon* beaconMsg; // we don't need a pointer to the header, we use methods of cPacket instead.
	CtpRoutingEngineMsg<OsModel_P, Radio_P> beaconMsg;

	/* routing table -- routing info about neighbors */
	routing_table_entry* routingTable;
	uint8_t routingTableActive;

	/* statistics */
	uint32_t parentChanges;
	/* end statistics */

	uint32_t routeUpdateTimerCount;

	uint32_t currentInterval;
	uint32_t t;
	bool tHasPassed;

	/////////////////////// Custom Variables //////////////////////////////

	// Pointers to other modules.
	//	CtpForwardingEngine *cfe;
	//	LinkEstimator *le;
	//	ResourceManager* resMgrModule;

	// Beacon Frame size.
	//	int ctpReHeaderSize;

	// Node Id.
	node_id_t self;

	// Sets a node as root from omnetpp.ini
	bool isRoot;

	// Arguments of generic module CtpRoutingEngineP
	uint32_t minInterval;
	uint32_t maxInterval;
	uint8_t routingTableSize;

	CtpRoutingEngine() {
	}
	~CtpRoutingEngine() {
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "Re: Destroyed\n" );
#endif
	}

	void init_variables(void) {
		//initialize RE parameters with default values
		maxInterval = 512000;
		minInterval = 128;
		//		ctpReHeaderSize = 5;
		isRoot = false;
		routingTableSize = 10;

		///////////////////// CtpForwardingEngine (default) /////////////////
		/////////////////////////////////////////////////////////////////////

		ECNOff = true;
		radioOn = true; // TO IMPLEMENT ------ radioOn in stdcontrol
		running = false;
		sending = false;
		justEvicted = false;
		//TODO: Allocate memory for routing table
		//			routingTable = new routing_table_entry[routingTableSize] ;

		currentInterval = minInterval;

		/////////////////////////// Init.init() /////////////////////////////

		routeUpdateTimerCount = 0;
		parentChanges = 0;
		state_is_root = 0;
	}

	int init(void) {

		init_variables();
		enable_radio();

		return SUCCESS;
	}

	int init(Radio& radio, Timer& timer, Debug& debug, Clock& clock,
			RandomNumber& random_number) {
		radio_ = &radio;
		timer_ = &timer;
		debug_ = &debug;
		clock_ = &clock;
		random_number_ = &random_number;

		init_variables();
		enable_radio();

		return SUCCESS;
	}

	int destruct(void) {
		return disable_radio();
	}

	///@name Routing Control
	///@{
	int enable_radio(void) {
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "Re: Boot for %d\n", radio().id() );
#endif

		radio().enable_radio();

		routeInfoInit(&routeInfo);
		routingTableInit();
		my_ll_addr = radio().id();
		self = radio().id();

		le.init();

		// Call the corresponding rootcontrol command
		isRoot ?
				command_RootControl_setRoot() : command_RootControl_unsetRoot();

		random_number().srand(clock().time() * (3 * radio().id() + 2));

		command_StdControl_start();

		radio().template reg_recv_callback<self_type, &self_type::receive>(
				this);

		timer().template set_timer<self_type, &self_type::timer_elapsed>(15000,
				this, 0);

		return SUCCESS;
	}
	int disable_radio(void) {
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "Re: Disable\n" );
#endif

		return command_StdControl_stop();
	}

	///@name Methods called by Timer
	///@{
	void timer_elapsed(void *userdata) {
		int timeout = (int) (userdata);

#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "Re: TimerFiredCallback, timeout: %d.\n", timeout );
#endif
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
			debug().debug( "Re: TimerFiredCallback unexpected timeout: %d\n", timeout );
#endif
			break;
		}
		}
	}
	///@}

	///@name Radio Concept
	///@{
	/**
	 */
	int send(node_id_t destination, size_t len, block_data_t *data) {

		//	RoutingTableIterator it = routing_table_.find(destination);
		//	if (it != routing_table_.end()) {
		//		routing_message_.set_path(it->second.path);
		//		radio().send(it->second.path[1], routing_message_.buffer_size(),
		//				(uint8_t*) &routing_message_);
		//#ifdef ROUTING_ENGINE_DEBUG
		//		debug().debug( "Re: Existing path in Cache with size %d hops %d idx %d\n",
		//				it->second.path.size(), it->second.hops, routing_message_.path_idx() );
		//		print_path( it->second.path );
		//#endif
		//	} else {
		//
		//		// radio().send( Radio::BROADCAST_ADDRESS, message.buffer_size(),
		//		// (uint8_t*) &message );
		//#ifdef ROUTING_ENGINE_DEBUG
		//		debug().debug( "Re: Start Route Request from %d to %d.\n", message.source(), message.destination() );
		//#endif
		//	}

		return SUCCESS;
	}
	/**
	 */
	void receive(node_id_t from, size_t len, block_data_t *data) {
		message_id_t msg_id = read<OsModel, block_data_t, message_id_t>(data);
		if (msg_id) {
			// RouteDiscoveryMessage *message =
			// reinterpret_cast<RouteDiscoveryMessage*> ( data );
			// handle_route_request( from, *message );
			// }
			// else if (msg_id == DsrRouteReplyMsgId)
			// {
			// RouteDiscoveryMessage *message =
			// reinterpret_cast<RouteDiscoveryMessage*> ( data );
			// handle_route_reply( from, *message );
			// }
			// else if (msg_id == ReMsgId)
			// {
			RoutingMessage *message = reinterpret_cast<RoutingMessage*>(data);
			event_BeaconReceive_receive(from, message);

			//TODO: implement logic for forwarding messages destinated to the FE
		}
	}

	typename Radio::node_id_t id() {
		return radio_->id();
	}
	///@}

	inline void routeInfoInit(route_info_t *ri) {
		ri->parent = INVALID_ADDR;
		ri->etx = 0;
		ri->haveHeard = 0;
		ri->congested = false;
	}

	/*
	 * The following functions are defined as inline to ease writing code during the development stage.
	 * They will be defined outside of the class before releasing.
	 */

	// -----------------------------------------------------------------------
	int setTimer(void *userdata, millis_t millis) {
		return timer().template set_timer<self_type, &self_type::timer_elapsed>(
				millis, this, userdata);
	}

	/* Methods for adjusting the Tricckle timeout interval */

	void chooseAdvertiseTime() {
		t = currentInterval;
		t /= 2;

		/* TODO: Need to clarify the use of the Random Wiselib concept */

		//	t += command_Random_rand32( 1 ) % t;
		t += random_number().rand(RANDOM_MAX) % t;
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

	error_t command_StdControl_start() {
		//start will (re)start the sending of messages
		if (!running) {
			running = true;
			resetInterval();
			setTimer((void*) ROUTE_TIMER, BEACON_INTERVAL);
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug( "RE: stdControl.start - running %b\n", running );
#endif
		}
		return SUCCESS;
	}

	error_t command_StdControl_stop() {
		running = false;
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "RE: stdControl.stop - running %b\n", running );
#endif
		return SUCCESS;
	}

	/* Is this quality measure better than the minimum threshold? */
	// Implemented assuming quality is EETX
	bool passLinkEtxThreshold(uint16_t etx) {
		return true;
		//    	return (etx < ETX_THRESHOLD);
	}

	/* Converts the output of the link estimator to path metric
	 * units, that can be *added* to form path metric measures */
	uint16_t evaluateEtx(uint16_t quality) {
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "evaluateEtx - %d -> %d\n", (int) quality, (int)(quality+10));
#endif
		return (quality + 10);
	}

	/* updates the routing information, using the info that has been received
	 * from neighbor beacons. Two things can cause this info to change:
	 * neighbor beacons, changes in link estimates, including neighbor eviction */
	void updateRouteTask() {
		uint8_t i;
		routing_table_entry * entry;
		routing_table_entry * best;
		uint16_t minEtx;
		uint16_t currentEtx;
		uint16_t linkEtx, pathEtx;

		if (state_is_root)
			return;

		best = NULL;
		/* Minimum etx found among neighbors, initially infinity */
		minEtx = MAX_METRIC;
		/* Metric through current parent, initially infinity */
		currentEtx = MAX_METRIC;

#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "updateRouteTask");
#endif

		/* Find best path in table, other than our current */
		for (i = 0; i < routingTableActive; i++) {
			entry = &routingTable[i];

			// Avoid bad entries and 1-hop loops
			if (entry->info.parent == INVALID_ADDR
					|| entry->info.parent == my_ll_addr) {
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug( "routingTable[%d]: neighbour: [id: %d, neighbour: %d, etx: NO ROUTE]\n", (int) i, (int) entry->neighbor , entry->info.parent);
#endif

				continue;
			}
			/* Compute this neighbor's path metric */
			linkEtx = evaluateEtx(
					le.command_LinkEstimator_getLinkQuality(entry->neighbor));

#ifdef ROUTING_ENGINE_DEBUG
			debug().debug( "routingTable[%d]: neighbour: [id: %d, neighbour: %d, etx: %d]\n", (int) i, (int) entry->neighbor , entry->info.parent,(int) linkEtx);
#endif
			pathEtx = linkEtx + entry->info.etx;
			/* Operations specific to the current parent */
			if (entry->neighbor == routeInfo.parent) {
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug( "already parent");
#endif
				currentEtx = pathEtx;
				/* update routeInfo with parent's current info */
				routeInfo.etx = entry->info.etx;
				routeInfo.congested = entry->info.congested;
				continue;
			}
			/* Ignore links that are congested */
			if (entry->info.congested)
				continue;
			/* Ignore links that are bad */
			if (!passLinkEtxThreshold(linkEtx)) {
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug( "did not pass threshold.");
#endif
				continue;
			}

			if (pathEtx < minEtx) {
				minEtx = pathEtx;
				best = entry;
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
				parentChanges++;
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug( "Changed parent from %d to %d",(int) routeInfo.parent, (int) best);
#endif
				le.command_LinkEstimator_unpinNeighbor(routeInfo.parent);
				le.command_LinkEstimator_pinNeighbor(best->neighbor);
				le.command_LinkEstimator_clearDLQ(best->neighbor);

				routeInfo.parent = best->neighbor;
				routeInfo.etx = best->info.etx;
				routeInfo.congested = best->info.congested;
			}
		}

		/* Finally, tell people what happened:  */
		/* We can only loose a route to a parent if it has been evicted. If it hasn't
		 * been just evicted then we already did not have a route */
		if (justEvicted && routeInfo.parent == INVALID_ADDR) {
			//TODO Callback to FE to signal no route found
//			signal_Routing_noRoute();
		}
		/* On the other hand, if we didn't have a parent (no currentEtx) and now we
		 * do, then we signal route found. The exception is if we just evicted the
		 * parent and immediately found a replacement route: we don't signal in this
		 * case */
		else if (!justEvicted && currentEtx == MAX_METRIC
				&& minEtx != MAX_METRIC) {
			//TODO Callback to FE to signal route found
//			signal_Routing_routeFound();
		}
		justEvicted = false;
	}

	/************************************************************/
	/* Routing Table Functions                                  */

	/* The routing table keeps info about neighbor's route_info,
	 * and is used when choosing a parent.
	 * The table is simple:
	 *   - not fragmented (all entries in 0..routingTableActive)
	 *   - not ordered
	 *   - no replacement: eviction follows the LinkEstimator table
	 */

	// these functions simulate the post command of TinyOs
	void post_updateRouteTask() {
		setTimer((void*) POST_UPDATEROUTETASK, 0); // cannot call the updateRouteTask directly. By this way it is more similar to the post command in TinyOs.
	}

	void post_sendBeaconTask() {
		setTimer((void*) POST_SENDBEACONTASK, 0);
	}

	/* send a beacon advertising this node's routeInfo */
	// only posted if running and radioOn
	void sendBeaconTask() {
		error_t eval;
		if (sending) {
			return;
		}

		beaconMsg.set_options(0);

		/* Congestion notification: am I congested? */
		//TODO Callback to FE
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
			//TODO Call get link quality from LE
			beaconMsg.set_etx(
					routeInfo.etx
							+ evaluateEtx(
									le.command_LinkEstimator_getLinkQuality(
											routeInfo.parent)));
		}

#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "sendBeaconTask - parent: %d etx: \n", (int) beaconMsg.parent(), (int) beaconMsg.etx());
#endif

		//TODO: Solve the lastHope issue
//		beaconMsg->getNetMacInfoExchange().lastHop = self; // ok

		//TODO: Call LE send command
//		eval = le->command_Send_send(BROADCAST_ADDRESS, beaconMsg); // the duplicate will be deleted in the LE module, we keep a copy here that is reused each time.

		if (eval == SUCCESS) {
			sending = true;
		} else {
			radioOn = false;
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug( "sendBeaconTask - running: %b, radioOn: %b \n",running, radioOn);
#endif
		}
	}

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
		debug().debug( "BeaconReceive.receive - from %d [parent: %d etx: %d] \n", (int) from, (int) (rcvBeacon->parent()), (int) (rcvBeacon->etx()) );
#endif

		//update neighbor table
		if (rcvBeacon->parent() != INVALID_ADDR) {

			/* If this node is a root, request a forced insert in the link
			 * estimator table and pin the node. */

			if (rcvBeacon->etx() == 0) {
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug( "from a root, inserting if not in table" );
#endif
				le.command_LinkEstimator_insertNeighbor(from);
				le.command_LinkEstimator_pinNeighbor(from);
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

	//TODO: This method should be invoked through callback from the LE
	void event_BeaconSend_sendDone(block_data_t* msg, error_t error) {
		if (!sending) {
			//something smells bad around here
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug( "event_BeaconSend_sendDone: something smells bad around here\n");
#endif
			return;
		}
		sending = false;
	}

	void event_RouteTimer_fired() {
		if (radioOn && running) {
			post_updateRouteTask();
		}
	}

	void event_BeaconTimer_fired() {
		if (radioOn && running) {
			if (!tHasPassed) {
				post_updateRouteTask(); // always the most up to date info
				post_sendBeaconTask();
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug( "Beacon timer fired.\n" );
#endif
				remainingInterval();
			} else {
				decayInterval();
			}
		}
	}

	void command_CtpInfo_setNeighborCongested(node_id_t n, bool congested) {
		uint8_t idx;
		if (ECNOff)
			return;
		idx = routingTableFind(n);
		if (idx < routingTableActive) {
			routingTable[idx].info.congested = congested;
		}
		if (routeInfo.congested && !congested)
			post_updateRouteTask();
		else if (routeInfo.parent == n && congested)
			post_updateRouteTask();
	}

	/*
	 *  RootControl Interface -----------------------------------------------------------
	 */
	/** sets the current node as a root, if not already a root */
	/*  returns FAIL if it's not possible for some reason      */
	error_t command_RootControl_setRoot() {
		bool route_found = false;
		route_found = (routeInfo.parent == INVALID_ADDR);
		state_is_root = 1;
		routeInfo.parent = my_ll_addr; //myself
		routeInfo.etx = 0;
		if (route_found) {

			//TODO: Implement callback to Forward Engine
			//			signal_Routing_routeFound();
		}
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "RootControl.setRoot - I'm a root now! %d\n", (int) routeInfo.parent );
#endif
		return SUCCESS;
	}

	error_t command_RootControl_unsetRoot() {
		state_is_root = 0;
		routeInfoInit(&routeInfo);
#ifdef ROUTING_ENGINE_DEBUG
		debug().debug( "RootControl.unsetRoot - I'm not a root now!\n" );
#endif

		post_updateRouteTask();
		return SUCCESS;
	}

	bool command_RootControl_isRoot() {
		return state_is_root;
	}

	/************************************************************/
	/* Routing Table Functions                                  */

	/* The routing table keeps info about neighbor's route_info,
	 * and is used when choosing a parent.
	 * The table is simple:
	 *   - not fragmented (all entries in 0..routingTableActive)
	 *   - not ordered
	 *   - no replacement: eviction follows the LinkEstimator table
	 ************************************************************/

	void routingTableInit() {
		routingTableActive = 0;
	}

	/* Returns the index of parent in the table or
	 * routingTableActive if not found */
	uint8_t routingTableFind(node_id_t neighbor) {
		uint8_t i;
		if (neighbor == INVALID_ADDR)
			return routingTableActive;
		for (i = 0; i < routingTableActive; i++) {
			if (routingTable[i].neighbor == neighbor)
				break;
		}
		return i;
	}

	error_t routingTableUpdateEntry(node_id_t from, node_id_t parent,
			uint16_t etx) {
		uint8_t idx;
		uint16_t linkEtx;
		linkEtx = evaluateEtx(le.command_LinkEstimator_getLinkQuality(from));

		idx = routingTableFind(from);
		if (idx == routingTableSize) {
			//not found and table is full
			//if (passLinkEtxThreshold(linkEtx))
			//TODO: add replacement here, replace the worst
			//}
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug( "routingTableUpdateEntry - FAIL, table full\n" );
#endif
			return ERR_BUSY;
		} else if (idx == routingTableActive) {
			//not found and there is space
			if (passLinkEtxThreshold(linkEtx)) {
				routingTable[idx].neighbor = from;
				routingTable[idx].info.parent = parent;
				routingTable[idx].info.etx = etx;
				routingTable[idx].info.haveHeard = 1;
				routingTable[idx].info.congested = false;
				routingTableActive++;
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug( "routingTableUpdateEntry - OK, new entry\n" );
#endif
			} else {
#ifdef ROUTING_ENGINE_DEBUG
				debug().debug( "routingTableUpdateEntry - Fail, link quality (%d) below threshold\n" ,(int)linkEtx);
#endif
			}
		} else {
			//found, just update
			routingTable[idx].neighbor = from;
			routingTable[idx].info.parent = parent;
			routingTable[idx].info.etx = etx;
			routingTable[idx].info.haveHeard = 1;
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug( "routingTableUpdateEntry - OK, updated entry\n" );
#endif
		}
		return SUCCESS;
	}

private:

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

	typename Radio::self_pointer_t radio_;
	typename Timer::self_pointer_t timer_;
	typename Debug::self_pointer_t debug_;
	typename Clock::self_pointer_t clock_;
	typename RandomNumber::self_pointer_t random_number_;

};

}
#endif /* __CTP_ROUTING_ENGINE_H__ */
