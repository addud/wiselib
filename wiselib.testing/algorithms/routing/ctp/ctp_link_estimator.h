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

#define LE_MAX_EVENT_RECEIVERS	2
#define NEIGHBOR_TABLE_SIZE			10

namespace wiselib {

template<typename OsModel_P, typename RoutingTable_P, typename RandomNumber_P,
		typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Debug_P = typename OsModel_P::Debug,
		typename Clock_P = typename OsModel_P::Clock>
class CtpLinkEstimator: public RoutingBase<OsModel_P, Radio_P> {
public:
	typedef OsModel_P OsModel;
	typedef RandomNumber_P RandomNumber;
	typedef RoutingTable_P RoutingTable;
	typedef Radio_P Radio;
	typedef Timer_P Timer;
	typedef Debug_P Debug;
	typedef typename OsModel::Clock Clock;

	typedef CtpLinkEstimator<OsModel, RoutingTable, RandomNumber, Radio, Timer,
			Debug, Clock> self_type;
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

	// --------------------------------------------------------------------

	// neighbor table entry
	typedef struct neighbor_table_entry {
		// link layer address of the neighbor
		node_id_t ll_addr;
		// last beacon sequence number received from this neighbor
		uint8_t lastseq;
		// number of beacons received after last beacon estimator update
		// the update happens every BLQ_PKT_WINDOW beacon packets
		uint8_t rcvcnt;
		// number of beacon packets missed after last beacon estimator update
		uint8_t failcnt;
		// flags to describe the state of this entry
		uint8_t flags;
		// MAXAGE-inage gives the number of update rounds we haven't been able
		// update the inbound beacon estimator
		uint8_t inage;
		// inbound qualities in the range [1..255]
		// 1 bad, 255 good
		uint8_t inquality;
		// EETX for the link to this neighbor. This is the quality returned to
		// the users of the link estimator
		uint16_t eetx;
		// Number of data packets successfully sent (ack'd) to this neighbor
		// since the last data estimator update round. This update happens
		// every DLQ_PKT_WINDOW data packets
		uint8_t data_success;
		// The total number of data packets transmission attempt to this neighbor
		// since the last data estimator update round.
		uint8_t data_total;
	} neighbor_table_entry_t;

	// ----------------------------------------------------------------------------------

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
		MSG_HEADER_SIZE = LinkEstimatorMsg::HEADER_SIZE, // LinkEstimator header overhead
		MAX_MESSAGE_LENGTH = Radio_P::MAX_MESSAGE_LENGTH - MSG_HEADER_SIZE, ///< Maximal number of bytes in payload minus the LE header and footer
		RANDOM_MAX = RandomNumber::RANDOM_MAX ///< Maximum random number that can be generated
	};

	// --------------------------------------------------------------------

	enum Events {
		LE_EVENT_NEIGHBOUR_EVICTED = 0, LE_EVENT_SHOULD_INSERT = 1
	};

	// --------------------------------------------------------------------

	// the NEIGHBOR_TABLE_SIZE = 10 has been removed since it is defined through omnetpp.ini

	// Masks for the flag field in the link estimation header
	enum {
		// use last four bits to keep track of
		// how many footer entries there are
		NUM_ENTRIES_FLAG = 15
	};

	// link estimator header added to
	// every message passing through the link estimator
	// linkest_header_t removed: fields are accessible through cPacket methods.

	// neighbor_stat_entry has been moved in the CtpNoePackets.msg packet definition. Consequently, also the linkest_footer is useless.

	// for outgoing link estimator message
	// so that we can compute bi-directional quality
	//typedef struct neighbor_stat_entry {
	//  am_addr_t ll_addr;
	//  uint8_t inquality;
	//} neighbor_stat_entry_t;

	// we put the above neighbor entry in the footer
	//typedef struct linkest_footer {
	//  neighbor_stat_entry_t neighborList[1];
	//} linkest_footer_t;

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
		INVALID_RVAL = 0xff,
		INVALID_NEIGHBOR_ADDR = 0xff,
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

	neighbor_table_entry_t NeighborTable[NEIGHBOR_TABLE_SIZE];
	// link estimation sequence, increment every time a beacon is sent
	uint8_t linkEstSeq;
	// if there is not enough room in the packet to put all the neighbor table
	// entries, in order to do round robin we need to remember which entry
	// we sent in the last beacon
	uint8_t prevSentIdx;

	// Node id
	node_id_t self;

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

		radio().enable_radio();

		radio().template reg_recv_callback<self_type, &self_type::receive>(
				this);

		//TODO: Move init functions to init_variables function
		random_number().srand(
				clock().milliseconds(clock().time()) * (3 * radio().id() + 2));

		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	int disable_radio(void) {

		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	node_id_t id() {
		return radio_->id();
	}

	// -----------------------------------------------------------------------

	int send(node_id_t destination, size_t len, block_data_t *data) {
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

	/*
	 * LinkEstimator Interface -------------------------------------------------------------
	 */
	// return bi-directional link quality to the neighbor
	uint16_t command_LinkEstimator_getLinkQuality(node_id_t neighbor) {
		uint8_t idx;
		idx = findIdx(neighbor);
		if (idx == INVALID_RVAL) {
			return VERY_LARGE_EETX_VALUE;
		} else {
			if (NeighborTable[idx].flags & MATURE_ENTRY) {
				return NeighborTable[idx].eetx;
			} else {
				return VERY_LARGE_EETX_VALUE;
			}
		}
	}

	// -----------------------------------------------------------------------

	// insert the neighbor at any cost (if there is a room for it)
	// even if eviction of a perfectly fine neighbor is called for
	error_t command_LinkEstimator_insertNeighbor(node_id_t neighbor) {
		uint8_t nidx;
//		echo("forced insert of %d", neighbor);
		nidx = findIdx(neighbor);
		if (nidx != INVALID_RVAL) {
#ifdef LINK_ESTIMATOR_DEBUG
			echo("insert: Found the entry, no need to insert");
#endif
			return SUCCESS;
		}

		nidx = findEmptyNeighborIdx();
		if (nidx != INVALID_RVAL) {
#ifdef LINK_ESTIMATOR_DEBUG
			echo("insert: inserted into the empty slot");
#endif
			initNeighborIdx(nidx, neighbor);
			return SUCCESS;
		} else {
			nidx = findWorstNeighborIdx(BEST_EETX);
			if (nidx != INVALID_RVAL) {
#ifdef LINK_ESTIMATOR_DEBUG
				echo("insert: inserted by replacing an entry for neighbor: %d",(int)NeighborTable[nidx].ll_addr);
#endif
				signal_LinkEstimator_evicted(NeighborTable[nidx].ll_addr);
				initNeighborIdx(nidx, neighbor);
				return SUCCESS;
			}
		}
		return ERR_BUSY;
	}

	// -----------------------------------------------------------------------

	// pin a neighbor so that it does not get evicted
	error_t command_LinkEstimator_pinNeighbor(node_id_t neighbor) {
		uint8_t nidx = findIdx(neighbor);
		if (nidx == INVALID_RVAL) {
			return ERR_UNSPEC;
		}
		NeighborTable[nidx].flags |= PINNED_ENTRY;
//		echo("pinned %d", (int) neighbor);
		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	// pin a neighbor so that it does not get evicted
	error_t command_LinkEstimator_unpinNeighbor(node_id_t neighbor) {
		uint8_t nidx = findIdx(neighbor);
		if (nidx == INVALID_RVAL) {
			return ERR_UNSPEC;
		}
		NeighborTable[nidx].flags &= ~PINNED_ENTRY;
//		echo("unpinned %d", (int) neighbor);
		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	// called by the RE as a feedback to the signal_CompareBit_shouldInsert event
	error_t command_LinkEstimator_forceInsertNeighbor(node_id_t neighbor) {
		uint8_t nidx;

		nidx = findRandomNeighborIdx();
		if (nidx == INVALID_RVAL) {
			return ERR_UNSPEC;
		}
		signal_LinkEstimator_evicted(NeighborTable[nidx].ll_addr);
		initNeighborIdx(nidx, neighbor);
		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	// called when the parent changes; clear state about data-driven link quality
	error_t command_LinkEstimator_clearDLQ(node_id_t neighbor) {
		neighbor_table_entry_t *ne;
		uint8_t nidx = findIdx(neighbor);
		if (nidx == INVALID_RVAL) {
			return ERR_UNSPEC;
		}
		ne = &NeighborTable[nidx];
		ne->data_total = 0;
		ne->data_success = 0;
		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	// user of link estimator calls send here
	// slap the header and footer before sending the message
	error_t command_Send_send(node_id_t addr, size_t size, block_data_t* pkt) {

		LinkEstimatorMsg lePkt; // initialize the LinkEstimator packet

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
				lePkt.HEADER_SIZE + lePkt.ne() * sizeof(neighbor_stat_entry)
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



		//TODO: Replace neighbor table
//		NEIGHBOR_TABLE_SIZE = 10;
//		NeighborTable = new neighbor_table_entry_t[NEIGHBOR_TABLE_SIZE];
		linkEstSeq = 0;
		prevSentIdx = 0;

		initNeighborTable();
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
				debug().debug("%d: LE: ", id());
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
		processReceivedMessage(from, len, msg);
		notify_receivers(from, len, msg->payload());
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
		uint8_t nidx;

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
		//   find an empty entry
		//   if found
		//     initialize the entry
		//   else
		//     find a bad neighbor to be evicted
		//     if found
		//       evict the neighbor and init the entry
		//     else
		//       we can not accommodate this neighbor in the table

		nidx = findIdx(ll_addr);

		if (nidx != INVALID_RVAL) {
#ifdef LINK_ESTIMATOR_DEBUG
			echo("Found the entry so updating");
#endif
//			echo("neighbour %d  found.  updating", ll_addr);
			updateNeighborEntryIdx(nidx, msg->seqno());
		} else {
//			echo("neighbour %d not found", ll_addr);
			nidx = findEmptyNeighborIdx();
			if (nidx != INVALID_RVAL) {

#ifdef LINK_ESTIMATOR_DEBUG
				echo("Found an empty entry");
#endif
				initNeighborIdx(nidx, ll_addr);
//				echo("empty entry. insert %d", ll_addr);
				updateNeighborEntryIdx(nidx, msg->seqno());
			} else {
				print_neighbor_table();
//				echo("no empty entry found");
				nidx = findWorstNeighborIdx(EVICT_EETX_THRESHOLD);
				if (nidx != INVALID_RVAL) {

#ifdef LINK_ESTIMATOR_DEBUG
					echo("Evicted neighbor %d  at idx %d",(int)NeighborTable[nidx].ll_addr,(int)nidx);
#endif
//					echo("no worst entry found for %d. signaling to the RE",
//							ll_addr);
					signal_LinkEstimator_evicted(NeighborTable[nidx].ll_addr);
					initNeighborIdx(nidx, ll_addr);
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

		// header and footer operations are skipped, we use cPacket methods instead.

		uint8_t j=0, k;
		uint8_t maxEntries, newPrevSentIdx=0;

		maxEntries = ((MAX_MESSAGE_LENGTH - MSG_HEADER_SIZE - len)
				/ sizeof(neighbor_stat_entry_t));

		for (int i = 0; i < NEIGHBOR_TABLE_SIZE && j < maxEntries; i++) {

			k = (prevSentIdx + i + 1) % NEIGHBOR_TABLE_SIZE;
			if ((NeighborTable[k].flags & VALID_ENTRY)
					&& (NeighborTable[k].flags & MATURE_ENTRY)) {

				neighbor_stat_entry_t temp;
				temp.ll_addr = NeighborTable[k].ll_addr;
				temp.inquality = NeighborTable[k].inquality;

				if (msg->add_neighbour_entry(j, (block_data_t*) &temp)
						!= SUCCESS) {
					echo("Could not add neighbour entry to LE message footer.");
					return ERR_UNSPEC;
				}

				newPrevSentIdx = k;

				j++;
			}
		}
		prevSentIdx = newPrevSentIdx;

		msg->set_seqno(linkEstSeq++);
		msg->set_ne(j);
		return SUCCESS;
	}

	// initialize the given entry in the table for neighbor ll_addr
	void initNeighborIdx(uint8_t i, node_id_t ll_addr) {
		neighbor_table_entry_t *ne;
		ne = &NeighborTable[i];
		ne->ll_addr = ll_addr;
		ne->lastseq = 0;
		ne->rcvcnt = 0;
		ne->failcnt = 0;
		ne->flags = (INIT_ENTRY | VALID_ENTRY);
		ne->inage = MAX_AGE;
		ne->inquality = 0;
		ne->eetx = 0;
	}

	// ----------------------------------------------------------------------------------

	// find the index to the entry for neighbor ll_addr
	uint8_t findIdx(node_id_t ll_addr) {
		uint8_t i;
		for (i = 0; i < NEIGHBOR_TABLE_SIZE; i++) {
			if (NeighborTable[i].flags & VALID_ENTRY) {
				if (NeighborTable[i].ll_addr == ll_addr) {
					return i;
				}
			}
		}
		return INVALID_RVAL;
	}

	// ----------------------------------------------------------------------------------

	// find an empty slot in the neighbor table
	uint8_t findEmptyNeighborIdx() {
		uint8_t i;
		for (i = 0; i < NEIGHBOR_TABLE_SIZE; i++) {
			if (NeighborTable[i].flags & VALID_ENTRY) {
			} else {
				return i;
			}
		}
		return INVALID_RVAL;
	}

	// ----------------------------------------------------------------------------------

	// find the index to the worst neighbor if the eetx
	// estimate is greater than the given threshold
	uint8_t findWorstNeighborIdx(uint8_t thresholdEETX) {
		uint8_t i, worstNeighborIdx;
		uint16_t worstEETX, thisEETX;

		worstNeighborIdx = INVALID_RVAL;
		worstEETX = 0;
		for (i = 0; i < NEIGHBOR_TABLE_SIZE; i++) {
			if (!(NeighborTable[i].flags & VALID_ENTRY)) {
				continue;
			}
			if (!(NeighborTable[i].flags & MATURE_ENTRY)) {
				continue;
			}
			if (NeighborTable[i].flags & PINNED_ENTRY) {
				continue;
			}
			thisEETX = NeighborTable[i].eetx;
			if (thisEETX >= worstEETX) {
				worstNeighborIdx = i;
				worstEETX = thisEETX;
			}
		}
		if (worstEETX >= thresholdEETX) {
			return worstNeighborIdx;
		} else {
			return INVALID_RVAL;
		}
	}

	// ----------------------------------------------------------------------------------

	// find the index to a random entry that is
	// valid but not pinned
	uint8_t findRandomNeighborIdx() {
		uint8_t i;
		uint8_t cnt;
		uint8_t num_eligible_eviction;

		num_eligible_eviction = 0;
		for (i = 0; i < NEIGHBOR_TABLE_SIZE; i++) {
			if (NeighborTable[i].flags & VALID_ENTRY) {
				if (NeighborTable[i].flags & PINNED_ENTRY
						|| NeighborTable[i].flags & MATURE_ENTRY) {
				} else {
					num_eligible_eviction++;
				}
			}
		}

		if (num_eligible_eviction == 0) {
			return INVALID_RVAL;
		}

		cnt = random_number().rand(num_eligible_eviction);

		for (i = 0; i < NEIGHBOR_TABLE_SIZE; i++) {
			if (!NeighborTable[i].flags & VALID_ENTRY)
				continue;
			if (NeighborTable[i].flags & PINNED_ENTRY
					|| NeighborTable[i].flags & MATURE_ENTRY)
				continue;
			if (cnt-- == 0)
				return i;
		}
		return INVALID_RVAL;
	}

	// ----------------------------------------------------------------------------------

	// update the EETX estimator
	// called when new beacon estimate is done
	// also called when new DEETX estimate is done
	void updateEETX(neighbor_table_entry_t *ne, uint16_t newEst) {
		//TODO: Use actual ETX instead of constant one
		ne->eetx = (ALPHA * ne->eetx + (10 - ALPHA) * newEst)/10;
//		ne->eetx = 1;
	}

	// ----------------------------------------------------------------------------------

	//TODO: updateDEETX like in original when TX acknowledged

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
		uint8_t i, totalPkt;
		neighbor_table_entry_t * ne;
		uint8_t newEst;
		uint8_t minPkt;

		minPkt = BLQ_PKT_WINDOW;
		for (i = 0; i < NEIGHBOR_TABLE_SIZE; i++) {
			ne = &NeighborTable[i];
			if (ne->ll_addr == n) {
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
	}

	// ----------------------------------------------------------------------------------

	// we received seq from the neighbor in idx
	// update the last seen seq, receive and fail count
	// refresh the age
	void updateNeighborEntryIdx(uint8_t idx, uint8_t seq) {
		uint8_t packetGap;

		if (NeighborTable[idx].flags & INIT_ENTRY) {
			NeighborTable[idx].lastseq = seq;
			NeighborTable[idx].flags &= ~INIT_ENTRY;
		}

		packetGap = seq - NeighborTable[idx].lastseq;
		NeighborTable[idx].lastseq = seq;
		NeighborTable[idx].rcvcnt++;
		NeighborTable[idx].inage = MAX_AGE;
		if (packetGap > 0) {
			NeighborTable[idx].failcnt += packetGap - 1;
		}
		if (packetGap > MAX_PKT_GAP) {
			NeighborTable[idx].failcnt = 0;
			NeighborTable[idx].rcvcnt = 1;
			NeighborTable[idx].inquality = 0;
		}

		if (NeighborTable[idx].rcvcnt >= BLQ_PKT_WINDOW) {
			updateNeighborTableEst(NeighborTable[idx].ll_addr);
		}

	}

	// ----------------------------------------------------------------------------------

	// print the neighbor table. for debugging.
	void print_neighbor_table() {
		uint8_t i;
		neighbor_table_entry_t *ne;
		for (i = 0; i < NEIGHBOR_TABLE_SIZE; i++) {
			ne = &NeighborTable[i];
			if (ne->flags & VALID_ENTRY) {
				echo(
						"%d:%d inQ=%d, inquality=%d, inA=%d, inage=%d, rcv=%d, fail=%d, failcnt=%d, Q=%d",
						(int) i, (int) ne->ll_addr, (int) ne->inquality,
						(int) ne->inage, (int) ne->rcvcnt, (int) ne->failcnt,
						(int) computeEETX(ne->inquality));
			}
		}
	}

	// ----------------------------------------------------------------------------------

	// initialize the neighbor table in the very beginning
	void initNeighborTable() {
		uint8_t i;

		for (i = 0; i < NEIGHBOR_TABLE_SIZE; i++) {
			NeighborTable[i].flags = 0;
		}
	}

}
;

// ----------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------
}
#endif /* __CTP_LINK_ESTIMATOR_H__ */
