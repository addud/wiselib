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

#ifndef __CTP_LINK_ESTIMATOR_H__
#define __CTP_LINK_ESTIMATOR_H__

#include "util/base_classes/routing_base.h"
#include "algorithms/routing/ctp/ctp_link_estimator_msg.h"
#include "algorithms/routing/ctp/ctp_types.h"
#include "algorithms/routing/ctp/ctp_debugging.h"

#include "internal_interface/routing_table/routing_table_static_array.h"
#include "algorithms/routing/ctp/ctp_neighbour_table_value.h"

#define LE_MAX_EVENT_RECEIVERS		2
#define NEIGHBOR_TABLE_SIZE			10

namespace wiselib {

template<typename OsModel_P, typename NeighbourTable_P, typename RandomNumber_P,
		typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Debug_P = typename OsModel_P::Debug,
		typename Clock_P = typename OsModel_P::Clock>
class CtpLinkEstimator: public RoutingBase<OsModel_P, Radio_P> {
public:
	typedef OsModel_P OsModel;
	typedef RandomNumber_P RandomNumber;
	//typedef NeighbourTable_P NeighbourTable;
	typedef Radio_P Radio;
	typedef Timer_P Timer;
	typedef Debug_P Debug;
	typedef typename OsModel::Clock Clock;

	typedef CtpLinkEstimator<OsModel, NeighbourTable_P, RandomNumber, Radio, Timer,
			Debug, Clock> self_type;
	typedef self_type* self_pointer_t;

	/*
	typedef typename NeighbourTable::mapped_type NeighbourTableValue;
	typedef NeighbourTableValue neighbor_table_entry_t;
	typedef typename NeighbourTable::iterator NeighbourTableIterator;
	*/

	typedef typename Radio::node_id_t node_id_t;
	typedef typename Radio::size_t size_t;
	typedef typename Radio::block_data_t block_data_t;
	typedef typename Radio::message_id_t message_id_t;

	typedef typename Timer::millis_t millis_t;
	typedef typename Clock::time_t time_t;
	typedef typename RandomNumber::value_t value_t;

	typedef delegate3<void, node_id_t, size_t, block_data_t*> radio_delegate_t;
	typedef vector_static<OsModel, radio_delegate_t, RADIO_BASE_MAX_RECEIVERS> RecvCallbackVector;
	typedef typename RecvCallbackVector::iterator RecvCallbackVectorIterator;

	typedef delegate3<void, uint8_t, node_id_t, block_data_t*> event_delegate_t;
	typedef vector_static<OsModel, event_delegate_t, LE_MAX_EVENT_RECEIVERS> EventCallbackVector;
	typedef typename EventCallbackVector::iterator EventCallbackVectorIterator;

	typedef struct neighbor_stat_entry {
		uint16_t ll_addr;
		uint8_t inquality;
	} neighbor_stat_entry_t;

	typedef CtpLinkEstimatorMsg<OsModel, Radio, sizeof(neighbor_stat_entry_t)> LinkEstimatorMsg;

	typedef CtpNeighbourTableValue<Radio> NeighbourTableValue;
	typedef StaticArrayRoutingTable<OsModel, Radio, 10, NeighbourTableValue> NeighbourTable;
	typedef typename NeighbourTable::iterator NeighbourTableIterator;


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
		//TODO: Compute max message length
		MAX_MESSAGE_LENGTH = Radio_P::MAX_MESSAGE_LENGTH -LinkEstimatorMsg::HEADER_SIZE, ///< Maximal number of bytes in payload minus the LE header and footer
		RANDOM_MAX = RandomNumber::RANDOM_MAX ///< Maximum random number that can be generated
	};

	// --------------------------------------------------------------------

	enum Events {
		LE_EVENT_NEIGHBOUR_EVICTED = 0, LE_EVENT_SHOULD_INSERT = 1
	};

	// --------------------------------------------------------------------

	// Masks for the flag field in the link estimation header
	enum {
		// use last four bits to keep track of
		// how many footer entries there are
		NUM_ENTRIES_FLAG = 15
	};

	// Flags for the neighbor table entry
	enum NeighbourTableEntryType {
		VALID_ENTRY = 0x1,
		// A link becomes mature after BLQ_PKT_WINDOW
		// packets are received and an estimate is computed
		MATURE_ENTRY = 0x2,
		// Flag to indicate that this link has received the
		// first sequence number
		INIT_ENTRY = 0x4,
		// The upper layer has requested that this link be pinned
		// Useful if we don't want to lose the root from the table
		PINNED_ENTRY = 0x8
	};

	// configure the link estimator and some constants
	enum LinkEstimatorConstants {
		// If the eetx estimate is below this threshold
		// do not evict a link
		EVICT_EETX_THRESHOLD = 55,
		// maximum link update rounds before we expire the link
		MAX_AGE = 6,
		// if received sequence number if larger than the last sequence
		// number by this gap, we reinitialize the link
		MAX_PKT_GAP = 10,
		BEST_EETX = 0,
		// if we don't know the link quality, we need to return a value so
		// large that it will not be used to form paths
		VERY_LARGE_EETX_VALUE = 0xff,
		// decay the link estimate using this alpha
		// we use a denominator of 10, so this corresponds to 0.2
		ALPHA = 9,
		// number of packets to wait before computing a new
		// DLQ (Data-driven Link Quality)
		DLQ_PKT_WINDOW = 5,
		// number of beacons to wait before computing a new
		// BLQ (Beacon-driven Link Quality)
		BLQ_PKT_WINDOW = 3,
		// largest EETX value that we feed into the link quality EWMA
		// a value of 60 corresponds to having to make six transmissions
		// to successfully receive one acknowledgement
		LARGE_EETX_VALUE = 60
	};

	// -----------------------------------------------------------------------
	// keep information about links from the neighbors

	// link estimation sequence, increment every time a beacon is sent
	uint8_t linkEstSeq;
	// if there is not enough room in the packet to put all the neighbour table
	// entries, in order to do round robin we need to remember which entry
	// we sent in the last beacon
	uint8_t prevSentIdx;

	// Node id
	node_id_t self;

	/* Neighbour table -- info about link to the neighbours */
	NeighbourTable nt;

	// -----------------------------------------------------------------------

	CtpLinkEstimator() {
	}

	// -----------------------------------------------------------------------

	~CtpLinkEstimator() {
#ifdef LINK_ESTIMATOR_DEBUG
		debug().debug( "LE: Destroyed\n" );
#endif
	}

	// -----------------------------------------------------------------------

	int init(void) {
		init_variables();

		return SUCCESS;
	}

	// -----------------------------------------------------------------------

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

	// -----------------------------------------------------------------------

	int enable_radio(void) {
#ifdef LINK_ESTIMATOR_DEBUG
		debug().debug( "LE: Boot for %d\n", radio().id() );
#endif

		radio().template reg_recv_callback<self_type, &self_type::receive>(
				this);

		//TODO: Move init functions to init_variables function
		random_number().srand(
				clock().milliseconds(clock().time()) * (3 * radio().id() + 2));

		return radio().enable_radio();
	}

	// -----------------------------------------------------------------------

	int disable_radio(void) {
		return radio().disable_radio();
	}

	// -----------------------------------------------------------------------

	node_id_t id() {
		return radio_->id();
	}

	// -----------------------------------------------------------------------

	int send(node_id_t destination, size_t len, block_data_t *data) {
//		debug().debug("LE sending from %d",radio().id());
		return command_Send_send(destination, len, data);
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

	// -----------------------------------------------------------------------

	template<class T, void (T::*TMethod)(uint8_t, node_id_t, block_data_t*)>
	uint8_t reg_event_callback(T *obj_pnt) {

		if (event_callbacks_.empty())
			event_callbacks_.assign(LE_MAX_EVENT_RECEIVERS, event_delegate_t());

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

	// ----------------------------------------------------------------------------------

	// return bi-directional link quality to the neighbour
	uint16_t command_LinkEstimator_getLinkQuality(node_id_t neighbour) {

		NeighbourTableIterator it;
		it = nt.find(neighbour);

		if (it == nt.end()) {
			return VERY_LARGE_EETX_VALUE;
		} else {
			if (it->second.flags & MATURE_ENTRY) {
				return it->second.eetx;
			} else {
				return VERY_LARGE_EETX_VALUE;
			}
			
		}
	}

	// -----------------------------------------------------------------------

	// insert the neighbor at any cost (if there is a room for it)
	// even if eviction of a perfectly fine neighbor is called for
	error_t command_LinkEstimator_insertNeighbor(node_id_t neighbor) {
		NeighbourTableIterator it;

		it = nt.find(neighbor);


		if (it != nt.end()) {
			return SUCCESS;
		}

		if (nt.size()!=nt.max_size()) {

			initNeighbourValue(&nt[neighbor]);
			return SUCCESS;

		} else {
			it = findWorstNeighbour(BEST_EETX);

			if (it!=nt.end()) {

			signal_LinkEstimator_evicted(it->first);
			
			it->first=neighbor;
			initNeighbourValue(&it->second);
			

			return SUCCESS;
			}
		}

		return ERR_BUSY;
	}

	// -----------------------------------------------------------------------

	// pin a neighbor so that it does not get evicted
	error_t command_LinkEstimator_pinNeighbor(node_id_t neighbor) {
		NeighbourTableIterator it;

		it=nt.find(neighbor);

		if (it!=nt.end()) {
			return ERR_UNSPEC;
		}

		it->second.flags |= PINNED_ENTRY;
//		echo("pinned %d", (int) neighbor);
		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	error_t command_LinkEstimator_unpinNeighbor(node_id_t neighbor) {
		NeighbourTableIterator it;

		it=nt.find(neighbor);

		if (it!=nt.end()) {
			return ERR_UNSPEC;
		}

		it->second.flags &= ~PINNED_ENTRY;
//		echo("unpinned %d", (int) neighbor);
		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	// called by the RE as a feedback to the signal_CompareBit_shouldInsert event
	error_t command_LinkEstimator_forceInsertNeighbor(node_id_t neighbor) {
		NeighbourTableIterator it;

		it = findRandomNeighbour();

		if (it!=nt.end()) {
			return ERR_UNSPEC;
		}

		signal_LinkEstimator_evicted(it->first);

		initNeighbourValue(&it->second);

		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	// called when the parent changes; clear state about data-driven link quality
	error_t command_LinkEstimator_clearDLQ(node_id_t neighbor) {
		NeighbourTableIterator it;

		it=nt.find(neighbor);

		if (it!=nt.end()) {
			return ERR_UNSPEC;
		}

		it->second.data_total = 0;
		it->second.data_success=0;

		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	// user of link estimator calls send here
	// slap the header and footer before sending the message
	error_t command_Send_send(node_id_t addr, size_t size, block_data_t* pkt) {

		LinkEstimatorMsg lePkt(CtpLinkEstimatorMsgId); // initialize the LinkEstimator packet

		// add the header and dynamically adds the size of the footer. Note that actually there is no footer but just a bigger header.
		if (addLinkEstHeaderAndFooter(&lePkt, size) != SUCCESS) {
			echo("Could not add header to LE message");
			return ERR_UNSPEC;
		}

		if (lePkt.set_payload(pkt, size) != SUCCESS) {
			echo("Could not add data to LE message.");
			return ERR_UNSPEC;
		}

		return radio().send(addr,
				LinkEstimatorMsg::HEADER_SIZE + lePkt.ne() * sizeof(neighbor_stat_entry)
						+ size, (block_data_t*) &lePkt);
	}

	// ----------------------------------------------------------------------------------

private:

	typename Radio::self_pointer_t radio_;
	typename Timer::self_pointer_t timer_;
	typename Debug::self_pointer_t debug_;
	typename Clock::self_pointer_t clock_;
	typename RandomNumber::self_pointer_t random_number_;

	RecvCallbackVector recv_callbacks_;
	EventCallbackVector event_callbacks_;

	// -----------------------------------------------------------------------

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

	// -----------------------------------------------------------------------

	void init_variables() {

		//Id of the node (like TOS_NODE_ID)
		self = radio().id();

		linkEstSeq = 0;
		prevSentIdx = 0;
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
				debug().debug("%d: LE: ", radio().id());
				debug().debug(buffer);
				debug().debug("\n");
				break;
			}
		}
	}

	// -----------------------------------------------------------------------

	void timer_elapsed(void *userdata) {

	}

	// -----------------------------------------------------------------------

	void receive(node_id_t from, size_t len, block_data_t *data) {
		if (from == radio().id()) {
			return;
		}


#ifdef CTP_DEBUGGING
		if (!areConnected(self, from)) {
			return;
		}
#endif

		LinkEstimatorMsg* msg = reinterpret_cast<LinkEstimatorMsg*>(data);

		if (msg->msg_id() != CtpLinkEstimatorMsgId) {
			return;
		}

		processReceivedMessage(from, len, msg);
		notify_receivers(from, len - LinkEstimatorMsg::HEADER_SIZE - msg->ne() * sizeof(neighbor_stat_entry), msg->payload());
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

	void notify_listeners(uint8_t event, node_id_t node, block_data_t *data) {
		for (EventCallbackVectorIterator it = event_callbacks_.begin();
				it != event_callbacks_.end(); ++it) {
			if (*it != event_delegate_t()) {
				(*it)(event, node, data);
			}

		}
	}

	// ----------------------------------------------------------------------------------

	// called when link estimator generator packet or
	// packets from upper layer that are wired to pass through
	// link estimator is received
	void processReceivedMessage(node_id_t ll_addr, size_t len,
			LinkEstimatorMsg *msg) {

		NeighbourTableIterator it;

#ifdef LINK_ESTIMATOR_DEBUG
		echo("Receiving packet.");
#endif

		//	print_packet(msg, len);

		//TODO: Should we process only routing beacon messages??
		/*Why not unicasts from FE too??
		 As a matter of fact, with the current Wiselib setup we can't find out
		 if the destination of the message unicast or broadcast, so we'll process
		 all messages indifferent whether they are data or routing beacons*/

		//if (command_SubAMPacket_destination(msg) == BROADCAST_ADDRESS) {
#ifdef LINK_ESTIMATOR_DEBUG
		echo("Got seq: %d from link: %d",(int)msg->seqno(),(int)ll_addr);
#endif

//		print_neighbor_table();

		// update neighbor table with this information
		// find the neighbor
		// if found
		//   update the entry
		// else
		//   if still room
		//     add an entry and initialize it
		//   else
		//     find a bad neighbor to be evicted
		//     if found
		//       evict the neighbor and init the entry
		//     else
		//       we can not accommodate this neighbor in the table

		it= nt.find(ll_addr);

		if (it!=nt.end()) {

#ifdef LINK_ESTIMATOR_DEBUG
			echo("Found the entry so updating");
#endif
//			echo("neighbour %d  found.  updating", ll_addr);
			updateNeighbourEntry(it, msg->seqno());
		} else {
//			echo("neighbour %d not found", ll_addr);
					//   find an empty entry

			if (nt.size() != nt.max_size()) {
#ifdef LINK_ESTIMATOR_DEBUG
				echo("Adding a new entry");
#endif
				initNeighbourValue(&nt[ll_addr]);
				updateNeighbourEntry(it, msg->seqno());
			} else {
				//print_neighbor_table();
//				echo("no empty entry found");
				it = findWorstNeighbour(EVICT_EETX_THRESHOLD);
				if (it != nt.end()) {
//					echo("no worst entry found for %d. signaling to the RE",
//							ll_addr);
					signal_LinkEstimator_evicted(it->first);
					initNeighbourValue(&it->second);
				} else {
//					echo("worst entry found for %d. should insert to RE",
//							ll_addr);

#ifdef LINK_ESTIMATOR_DEBUG
					echo("No room in the table");
#endif
					//TODO: Callback to RE to signal Should Insert
					//This is a bit more complicated - needs feedback from the RE to see if the neighbour must be inserted or not
					//Maybe the neighbour can be stored somewhere and it can be inserted later, at the direct call by the RE when it receives the callback

					signal_CompareBit_shouldInsert(ll_addr, msg->payload());
				}
			}
		}
	}

	// ----------------------------------------------------------------------------------

	/*
	 * Signals to the RE
	 */

	//TODO: signal RE that a neighbour was evicted
	void signal_LinkEstimator_evicted(node_id_t n) {
		notify_listeners(LE_EVENT_NEIGHBOUR_EVICTED, n, NULL);
	}

	void signal_CompareBit_shouldInsert(node_id_t ll_addr, block_data_t* msg) {
		notify_listeners(LE_EVENT_SHOULD_INSERT, ll_addr, msg);
	}

	// ----------------------------------------------------------------------------------

	// add the link estimation header (seq no) and link estimation
	// footer (neighbor entries) in the packet. Call just before sending
	// the packet.
	error_t addLinkEstHeaderAndFooter(LinkEstimatorMsg *msg, uint8_t len) {

		NeighbourTableIterator it;
		uint8_t j=0, k;
		uint8_t maxEntries, newPrevSentIdx=0;

		maxEntries = ((MAX_MESSAGE_LENGTH - LinkEstimatorMsg::HEADER_SIZE - len)
				/ sizeof(neighbor_stat_entry_t));


		//TODO: wrap around to send all entries in a round robin fashion

		for (it=nt.begin();it!=nt.end() && j<maxEntries;it++) {
			if (it->second.flags & (VALID_ENTRY|MATURE_ENTRY)) {
				neighbor_stat_entry_t temp;
				temp.ll_addr = it->first;
				temp.inquality = it->second.inquality;

				if (msg->add_neighbour_entry(j, (block_data_t*) &temp)
						!= SUCCESS) {
					echo("Could not add neighbour entry to LE message footer.");
					return ERR_UNSPEC;
				}
				j++;
			}
		}

		//for (int i = 0; i < NEIGHBOR_TABLE_SIZE && j < maxEntries; i++) {

		//	k = (prevSentIdx + i + 1) % NEIGHBOR_TABLE_SIZE;
		//	if ((NeighborTable[k].flags & VALID_ENTRY)
		//			&& (NeighborTable[k].flags & MATURE_ENTRY)) {

		//		neighbor_stat_entry_t temp;
		//		temp.ll_addr = NeighborTable[k].ll_addr;
		//		temp.inquality = NeighborTable[k].inquality;

		//		if (msg->add_neighbour_entry(j, (block_data_t*) &temp)
		//				!= SUCCESS) {
		//			echo("Could not add neighbour entry to LE message footer.");
		//			return ERR_UNSPEC;
		//		}

		//		newPrevSentIdx = k;

		//		j++;
		//	}
		//}
		//prevSentIdx = newPrevSentIdx;

		msg->set_seqno(linkEstSeq++);
		msg->set_ne(j);
		return SUCCESS;
	}

	// initialize the given entry in the table for neighbor ll_addr
	void initNeighbourValue(NeighbourTableValue *ne) {
		ne->lastseq = 0;
		ne->rcvcnt = 0;
		ne->failcnt = 0;
		ne->flags = (INIT_ENTRY | VALID_ENTRY);
		ne->inage = MAX_AGE;
		ne->inquality = 0;
		ne->eetx = 0;
	}
	
	// ----------------------------------------------------------------------------------

	// find the worst neighbor if the eetx
	// estimate is greater than the given threshold
	NeighbourTableIterator findWorstNeighbour(uint8_t thresholdEETX) {

		NeighbourTableIterator it, worstNeighbour;
		uint16_t worstEETX, thisEETX;
	
		worstNeighbour = nt.end();
		worstEETX = 0;

		for (it = nt.begin(); it != nt.end(); it++) {

			if (it->second.flags & (VALID_ENTRY|MATURE_ENTRY|PINNED_ENTRY)) {
				
				thisEETX = it->second.eetx;

				if (thisEETX >= worstEETX) {
					worstNeighbour = it;
					worstEETX = thisEETX;
				}

			}

		}

		if (worstEETX >= thresholdEETX) {

			return worstNeighbour;

		} else {

			return nt.end();

		}
	}

	// ----------------------------------------------------------------------------------

	// find a neighbour entry that is
	// valid but not pinned
	bool eligibleForRemoval(NeighbourTableIterator it) {
		if (it->second.flags & VALID_ENTRY && !(it->second.flags & (PINNED_ENTRY | MATURE_ENTRY))) {
			return true;
		}
		return false;
	}

	NeighbourTableIterator findRandomNeighbour() {
		NeighbourTableIterator it;
		uint8_t cnt;
		uint8_t num_eligible_eviction;

		num_eligible_eviction = 0;

		//count the number of eligible neighbours for removal
		for (it = nt.begin(); it != nt.end(); it++) {
			if (eligibleForRemoval(it)) {
				num_eligible_eviction++;
			}
		}

		if (num_eligible_eviction == 0) {
			return nt.end();
		}

		cnt = random_number().rand(num_eligible_eviction);

		//parse the table to find the generated random neighbour 
		for (it = nt.begin(); it != nt.end(); it++) {
			if (eligibleForRemoval(it)) {
				if (cnt--==0) {
					return it;
				}
			}
		}

		return nt.end();
	}

	// ----------------------------------------------------------------------------------

	// update the EETX estimator
	// called when new beacon estimate is done
	// also called when new DEETX estimate is done
	void updateEETX(NeighbourTableValue *ne, uint16_t newEst) {
		ne->eetx = (ALPHA * ne->eetx + (10 - ALPHA) * newEst)/10;
	}

	// ----------------------------------------------------------------------------------

	//TODO: updateDEETX (Data driven ETX) when TX acknowledged

	// EETX (Extra Expected number of Transmission)
	// EETX = ETX - 1
	// computeEETX returns EETX*10
	uint8_t computeEETX(uint8_t q1) {
		uint16_t q;
		if (q1 > 0) {
			q = 2550 / q1 - 10;
			if (q > 255) {
				q = VERY_LARGE_EETX_VALUE;
			}
			return (uint8_t) q;
		} else {
			return VERY_LARGE_EETX_VALUE;
		}
	}

	// ----------------------------------------------------------------------------------

	// update the inbound link quality by
	// munging receive, fail count since last update
	void updateNeighborTableEst(node_id_t n) {
		NeighbourTableIterator it;
		uint8_t totalPkt;
		NeighbourTableValue *ne;
		uint8_t newEst;
		uint8_t minPkt;

		minPkt = BLQ_PKT_WINDOW;

		it = nt.find(n);

		if (it!=nt.end()) {
				ne=&it->second;
				if (ne->flags & VALID_ENTRY) {
					if (ne->inage > 0)
						ne->inage--;

					if (ne->inage == 0) {
						ne->flags ^= VALID_ENTRY;
						ne->inquality = 0;
					} else {
						ne->flags |= MATURE_ENTRY;
						totalPkt = ne->rcvcnt + ne->failcnt;
						if (totalPkt < minPkt) {
							totalPkt = minPkt;
						}
						if (totalPkt == 0) {
							ne->inquality = (ALPHA * ne->inquality) / 10;
						} else {
							newEst = (255 * ne->rcvcnt) / totalPkt;
							ne->inquality = (ALPHA * ne->inquality
									+ (10 - ALPHA) * newEst) / 10;
						}
						ne->rcvcnt = 0;
						ne->failcnt = 0;
					}
					updateEETX(ne, computeEETX(ne->inquality));
				} else {
#ifdef LINK_ESTIMATOR_DEBUG
					echo("- entry: %d is invalid", i);
#endif
				}
			}
	}

	// ----------------------------------------------------------------------------------

	// we received seq from the neighbor pointed by it
	// update the last seen seq, receive and fail count
	// refresh the age
	void updateNeighbourEntry(NeighbourTableIterator it, uint8_t seq) {
		NeighbourTableValue *ne;
		uint8_t packetGap;

		ne=&it->second;

		if (ne->flags & INIT_ENTRY) {
			ne->lastseq = seq;
			ne->flags &= ~INIT_ENTRY;
		}

		packetGap = seq - ne->lastseq;
		ne->lastseq = seq;
		ne->rcvcnt++;
		ne->inage = MAX_AGE;
		if (packetGap > 0) {
			ne->failcnt += packetGap - 1;
		}
		if (packetGap > MAX_PKT_GAP) {
			ne->failcnt = 0;
			ne->rcvcnt = 1;
			ne->inquality = 0;
		}

		if (ne->rcvcnt >= BLQ_PKT_WINDOW) {
			updateNeighborTableEst(it->first);
		}
	}

	// ----------------------------------------------------------------------------------

	// print the neighbor table. for debugging.
	//void print_neighbor_table() {
	//	uint8_t i;
	//	neighbor_table_entry_t *ne;
	//	for (i = 0; i < NEIGHBOR_TABLE_SIZE; i++) {
	//		ne = &NeighborTable[i];
	//		if (ne->flags & VALID_ENTRY) {
	//			echo(
	//					"%d:%d inQ=%d, inquality=%d, inA=%d, inage=%d, rcv=%d, fail=%d, failcnt=%d, Q=%d",
	//					(int) i, (int) ne->ll_addr, (int) ne->inquality,
	//					(int) ne->inage, (int) ne->rcvcnt, (int) ne->failcnt,
	//					(int) computeEETX(ne->inquality));
	//		}
	//	}
	//}

	// ----------------------------------------------------------------------------------
}
;

// ----------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------
}
#endif /* __CTP_LINK_ESTIMATOR_H__ */
