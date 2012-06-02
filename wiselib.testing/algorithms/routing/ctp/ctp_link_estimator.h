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
#include "ctp_routing_engine_msg.h"
//#include "ctp_link_estimator.h"
#include "ctp_types.h"

namespace wiselib {

template<typename OsModel_P, typename RoutingTable_P,
		typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Debug_P = typename OsModel_P::Debug>
class CtpLinkEstimator: public RoutingBase<OsModel_P, Radio_P> {
public:
	typedef OsModel_P OsModel;
	typedef Radio_P Radio;
	typedef typename OsModel::Timer Timer;
	typedef typename OsModel::Debug Debug;
	typedef typename OsModel::Clock Clock;
	typedef typename OsModel::RandomNumber RandomNumber;

	typedef RoutingTable_P RoutingTable;
	typedef typename RoutingTable::iterator RoutingTableIterator;
	typedef typename RoutingTable::mapped_type RoutingTableValue;
	typedef typename RoutingTable::value_type RoutingTableEntry;

	typedef typename RoutingTableValue::Path Path;
	typedef typename Path::iterator PathIterator;

	typedef CtpLinkEstimator<OsModel, RoutingTable, Radio> self_type;
	typedef self_type* self_pointer_t;

	typedef typename Radio::node_id_t node_id_t;
	typedef typename Radio::size_t size_t;
	typedef typename Radio::block_data_t block_data_t;
	typedef typename Radio::message_id_t message_id_t;

	typedef typename Timer::millis_t millis_t;
	typedef typename Clock::time_t time_t;
	typedef typename RandomNumber::value_t value_t;

	typedef CtpRoutingEngineMsg<OsModel, Radio> RoutingMessage;
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
		NULL_NODE_ID = Radio_P::NULL_NODE_ID 	///< Unknown/No node id
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
		POST_SENDBEACONTASK = 4,
	};

	enum TreeRouting {
		AM_TREE_ROUTING_CONTROL = 0xCE,
		BEACON_INTERVAL = 8192,
		INVALID_ADDR = NULL_NODE_ID,
		ETX_THRESHOLD = 50, // link quality=20% -> ETX=5 -> Metric=50
		PARENT_SWITCH_THRESHOLD = 15,
		MAX_METRIC = 0xFFFF,
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

	// --------------------------------------------------------------------
	///@name Construction / Destruction
	///@{
	CtpLinkEstimator();
	~CtpLinkEstimator();
	///@}

	int init(Radio& radio, Timer& timer, Debug& debug, Clock& clock, RandomNumber& random_number) {
		radio_ = &radio;
		timer_ = &timer;
		debug_ = &debug;
		clock_ = &clock;
		random_number_ = &random_number;

		random_number().srand(clock(). time( NULL ) * ( 3 * radio().id() + 2 ) );

		return SUCCESS;
	}

	inline int init();
	inline int destruct();

	///@name Routing Control
	///@{
	int enable_radio(void);
	int disable_radio(void);
	///@}

	///@name Methods called by Timer
	///@{
	void timer_elapsed(void *userdata);
	///@}

	///@name Radio Concept
	///@{
	/**
	 */
	int send(node_id_t receiver, size_t len, block_data_t *data);
	/**
	 */
	void receive(node_id_t from, size_t len, block_data_t *data);
	/**
	 */
	typename Radio::node_id_t id() {
		return radio_->id();
	}
	///@}

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
	typename Debug::self_pointer_t clock_;
	typename RandomNumber::self_pointer_t random_number_;

	/*
	 * LinkEstimator Interface -------------------------------------------------------------
	 */
	// return bi-directional link quality to the neighbor
	uint16_t LinkEstimator::command_LinkEstimator_getLinkQuality(am_addr_t neighbor) {
//		uint8_t idx;
//		idx = findIdx(neighbor);
//		if (idx == INVALID_RVAL) {
//			return VERY_LARGE_EETX_VALUE;
//		} else {
//			if (NeighborTable[idx].flags & MATURE_ENTRY) {
//				return NeighborTable[idx].eetx;
//			} else {
//				return VERY_LARGE_EETX_VALUE;
//			}
//		}
		return 1;
	}

	// pin a neighbor so that it does not get evicted
	error_t command_LinkEstimator_pinNeighbor(am_addr_t neighbor) {
//			uint8_t nidx = findIdx(neighbor);
//			if (nidx == INVALID_RVAL) {
//				return FAIL;
//			}
//			NeighborTable[nidx].flags |= PINNED_ENTRY;
		return SUCCESS;
	}

	// pin a neighbor so that it does not get evicted
	error_t command_LinkEstimator_unpinNeighbor(am_addr_t neighbor) {
//		uint8_t nidx = findIdx(neighbor);
//		if (nidx == INVALID_RVAL) {
//			return FAIL;
//		}
//		NeighborTable[nidx].flags &= ~PINNED_ENTRY;
		return SUCCESS;
	}


};

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
template<typename OsModel_P, typename RoutingTable_P, typename Radio_P,
		typename Timer_P, typename Debug_P>
CtpLinkEstimator<OsModel_P, RoutingTable_P, Radio_P, Timer_P, Debug_P>::CtpLinkEstimator() {
}
// -----------------------------------------------------------------------
template<typename OsModel_P, typename RoutingTable_P, typename Radio_P,
		typename Timer_P, typename Debug_P>
CtpLinkEstimator<OsModel_P, RoutingTable_P, Radio_P, Timer_P, Debug_P>::~CtpLinkEstimator() {
#ifdef ROUTING_ENGINE_DEBUG
	debug().debug( "Re: Destroyed\n" );
#endif
}
// -----------------------------------------------------------------------
template<typename OsModel_P, typename RoutingTable_P, typename Radio_P,
		typename Timer_P, typename Debug_P>
int CtpLinkEstimator<OsModel_P, RoutingTable_P, Radio_P, Timer_P, Debug_P>::init(
		void) {

	enable_radio();

	return SUCCESS;
}
// -----------------------------------------------------------------------
template<typename OsModel_P, typename RoutingTable_P, typename Radio_P,
		typename Timer_P, typename Debug_P>
int CtpLinkEstimator<OsModel_P, RoutingTable_P, Radio_P, Timer_P, Debug_P>::destruct(
		void) {
	return disable_radio();
}
// -----------------------------------------------------------------------
template<typename OsModel_P, typename RoutingTable_P, typename Radio_P,
		typename Timer_P, typename Debug_P>
int CtpLinkEstimator<OsModel_P, RoutingTable_P, Radio_P, Timer_P, Debug_P>::enable_radio(
		void) {
#ifdef ROUTING_ENGINE_DEBUG
	debug().debug( "Re: Boot for %d\n", radio().id() );
#endif

	radio().enable_radio();


	radio().template reg_recv_callback<self_type, &self_type::receive>(this);

	timer().template set_timer<self_type, &self_type::timer_elapsed>(15000,
			this, 0);

	return SUCCESS;
}
// -----------------------------------------------------------------------
template<typename OsModel_P, typename RoutingTable_P, typename Radio_P,
		typename Timer_P, typename Debug_P>
int CtpLinkEstimator<OsModel_P, RoutingTable_P, Radio_P, Timer_P, Debug_P>::disable_radio(
		void) {

	return SUCCESS;
}
// -----------------------------------------------------------------------
template<typename OsModel_P, typename RoutingTable_P, typename Radio_P,
		typename Timer_P, typename Debug_P>
void CtpLinkEstimator<OsModel_P, RoutingTable_P, Radio_P, Timer_P, Debug_P>::timer_elapsed(
		void *userdata) {

}
// -----------------------------------------------------------------------
template<typename OsModel_P, typename RoutingTable_P, typename Radio_P,
		typename Timer_P, typename Debug_P>
int CtpLinkEstimator<OsModel_P, RoutingTable_P, Radio_P, Timer_P, Debug_P>::send(
		node_id_t destination, size_t len, block_data_t *data) {

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
// -----------------------------------------------------------------------
template<typename OsModel_P, typename RoutingTable_P, typename Radio_P,
		typename Timer_P, typename Debug_P>
void CtpLinkEstimator<OsModel_P, RoutingTable_P, Radio_P, Timer_P, Debug_P>::receive(
		node_id_t from, size_t len, block_data_t *data) {
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
		handle_routing_message(from, len, *message);
	}
}

}
#endif /* __CTP_ROUTING_ENGINE_H__ */
