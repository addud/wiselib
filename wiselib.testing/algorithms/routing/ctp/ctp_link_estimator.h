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
#include "algorithms/routing/ctp/ctp_routing_engine_msg.h"
#include "algorithms/routing/ctp/ctp_types.h"

namespace wiselib {

	template<typename OsModel_P, typename RandomNumber_P,
		typename Radio_P = typename OsModel_P::Radio,
		typename Timer_P = typename OsModel_P::Timer,
		typename Debug_P = typename OsModel_P::Debug,
		typename Clock_P = typename OsModel_P::Clock>
	class CtpLinkEstimator: public RoutingBase<OsModel_P, Radio_P> {
	public:
		typedef OsModel_P OsModel;
		typedef RandomNumber_P RandomNumber;
		typedef Radio_P Radio;
		typedef Timer_P Timer;
		typedef Debug_P Debug;
		typedef typename OsModel::Clock Clock;

		typedef CtpLinkEstimator<OsModel, RandomNumber, Radio, Timer, Debug, Clock> self_type;
		typedef self_type* self_pointer_t;

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

		typedef delegate1<void, uint8_t> notify_delegate_t;
		typedef vector_static<OsModel, notify_delegate_t, MAX_EVENT_RECEIVERS> EventCallbackVector;
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
			MAX_MESSAGE_LENGTH = Radio_P::MAX_MESSAGE_LENGTH, ///< Maximal number of bytes in payload
			RANDOM_MAX = RandomNumber::RANDOM_MAX ///< Maximum random number that can be generated
		};

		// --------------------------------------------------------------------

		uint16_t link_quality;

		// -----------------------------------------------------------------------

		CtpLinkEstimator() {
		}

		// -----------------------------------------------------------------------

		~CtpLinkEstimator() {
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug( "Re: Destroyed\n" );
#endif
		}

		// -----------------------------------------------------------------------

		int init(void) {

			enable_radio();

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
				switch (radio_->id()) {
				case 0:
					link_quality = 2;
					break;
				case 1:
					link_quality = 3;
					break;
				case 2:
					link_quality = 4;
					break;
				case 3:
					link_quality = 3;
					break;
				case 4:
					link_quality = 2;
					break;
				case 5:
					link_quality = 1;
					break;
				case 6:
					link_quality = 0;
					break;
				case 7:
					link_quality = 1;
					break;
				default:
					link_quality = 100;
					break;
				}
				enable_radio();

				return SUCCESS;
		}

		// -----------------------------------------------------------------------

		int enable_radio(void) {
#ifdef ROUTING_ENGINE_DEBUG
			debug().debug( "Re: Boot for %d\n", radio().id() );
#endif

			radio().enable_radio();

			radio().reg_recv_callback<CtpLinkEstimator, &CtpLinkEstimator::receive>(
					this);

			random_number().srand(clock().time() * (3 * radio().id() + 2));

			//		radio().template reg_recv_callback<self_type, &self_type::receive>(
			//				this);
			//
			//		timer().template set_timer<self_type, &self_type::timer_elapsed>(15000,
			//				this, 0);

			return SUCCESS;
		}

		// -----------------------------------------------------------------------

		int disable_radio(void) {

			return SUCCESS;
		}

		// ----------------------------------------------------------------------------------

		node_id_t id() {
			return radio_->id();
		}

		// -----------------------------------------------------------------------

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

	      template<class T, void (T::*TMethod)(node_id_t, size_t, block_data_t*)>
	      int reg_recv_callback( T *obj_pnt )
	      {
	         if ( recv_callbacks_.empty() )
	            recv_callbacks_.assign( RADIO_BASE_MAX_RECEIVERS, radio_delegate_t() );

	         for ( unsigned int i = 0; i < recv_callbacks_.size(); ++i )
	         {
	            if ( recv_callbacks_.at(i) == radio_delegate_t() )
	            {
	               recv_callbacks_.at(i) = radio_delegate_t::template from_method<T, TMethod>( obj_pnt );
	               return i;
	            }
	         }

	         return -1;
	      }
	      // --------------------------------------------------------------------
	      int unreg_recv_callback( int idx )
	      {
	         recv_callbacks_.at(idx) = radio_delegate_t();
	         return SUCCESS;
	      }

		// -----------------------------------------------------------------------

		/*
		* LinkEstimator Interface -------------------------------------------------------------
		*/
		// return bi-directional link quality to the neighbor
		uint16_t command_LinkEstimator_getLinkQuality(node_id_t neighbor) {
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
			//		debug().debug("%d: link quality: %d\n",radio().id(),link_quality);
			return 1;
		}

		// -----------------------------------------------------------------------

		// insert the neighbor at any cost (if there is a room for it)
		// even if eviction of a perfectly fine neighbor is called for
		error_t command_LinkEstimator_insertNeighbor(node_id_t neighbor) {

			//		nidx = findIdx(neighbor);
			//		if (nidx != INVALID_RVAL) {
			//			trace()<<"insert: Found the entry, no need to insert";
			//			return SUCCESS;
			//		}
			//
			//		nidx = findEmptyNeighborIdx();
			//		if (nidx != INVALID_RVAL) {
			//			trace()<<"insert: inserted into the empty slot";
			//			initNeighborIdx(nidx, neighbor);
			//			return SUCCESS;
			//		} else {
			//			nidx = findWorstNeighborIdx(BEST_EETX);
			//			if (nidx != INVALID_RVAL) {
			//				trace()<<"insert: inserted by replacing an entry for neighbor: "<<(int)NeighborTable[nidx].ll_addr ;
			//				signal_LinkEstimator_evicted(NeighborTable[nidx].ll_addr) ;
			//				initNeighborIdx(nidx, neighbor);
			//				return SUCCESS;
			//			}
			//		}
			return ERR_BUSY;
		}

		// -----------------------------------------------------------------------

		// pin a neighbor so that it does not get evicted
		error_t command_LinkEstimator_pinNeighbor(node_id_t neighbor) {
			//			uint8_t nidx = findIdx(neighbor);
			//			if (nidx == INVALID_RVAL) {
			//				return FAIL;
			//			}
			//			NeighborTable[nidx].flags |= PINNED_ENTRY;
			return SUCCESS;
		}

		// -----------------------------------------------------------------------

		// pin a neighbor so that it does not get evicted
		error_t command_LinkEstimator_unpinNeighbor(node_id_t neighbor) {
			//		uint8_t nidx = findIdx(neighbor);
			//		if (nidx == INVALID_RVAL) {
			//			return FAIL;
			//		}
			//		NeighborTable[nidx].flags &= ~PINNED_ENTRY;
			return SUCCESS;
		}

		// -----------------------------------------------------------------------

		// called when the parent changes; clear state about data-driven link quality
		error_t command_LinkEstimator_clearDLQ(node_id_t neighbor) {
			//		neighbor_table_entry_t *ne;
			//		uint8_t nidx = findIdx(neighbor);
			//		if (nidx == INVALID_RVAL) {
			//			return FAIL;
			//		}
			//		ne = &NeighborTable[nidx];
			//		ne->data_total = 0;
			//		ne->data_success = 0;
			return SUCCESS;
		}

		// -----------------------------------------------------------------------

		error_t command_Send_send(node_id_t dest, size_t len, block_data_t *data ) {
			return radio().send(dest, len, data);
		}

		// -----------------------------------------------------------------------

		// ----------------------------------------------------------------------------------

		template<class T, void(T::*TMethod)(uint8_t) >
		uint8_t reg_event_callback(T *obj_pnt) {

			if (event_callbacks_.empty())
				event_callbacks_.assign(MAX_EVENT_RECEIVERS, notify_delegate_t());

			for (EventCallbackVectorIterator
				it = event_callbacks_.begin();
				it != event_callbacks_.end();
			it++) {
				if ((*it) == notify_delegate_t()) {
					(*it) = notify_delegate_t::template from_method<T, TMethod > (obj_pnt);
					return 0;
				}
			}

			return -1;
		}

		// ----------------------------------------------------------------------------------

		int unreg_event_callback(int idx) {
			event_callbacks_.at(idx) = notify_delegate_t();
			return idx;
		}

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

	      // --------------------------------------------------------------------
	      void notify_receivers( node_id_t from, size_t len, block_data_t *data )
	      {
	         for ( RecvCallbackVectorIterator
	                  it = recv_callbacks_.begin();
	                  it != recv_callbacks_.end();
	                  ++it )
	         {
	            if ( *it != radio_delegate_t() )
	               (*it)( from, len, data );
	         }
	      }

		// ----------------------------------------------------------------------------------

		void notify_listeners(uint8_t event) {
			for (EventCallbackVectorIterator it = event_callbacks_.begin();
				it != event_callbacks_.end(); ++it) {
					if (*it != notify_delegate_t()) {
						(*it)(event);
						echo("RE notified: %d\n",it);
					}

			}
		}

		// -----------------------------------------------------------------------

		void timer_elapsed(void *userdata) {

		}

		// -----------------------------------------------------------------------

		void receive(node_id_t from, size_t len, block_data_t *data) {
			notify_receivers(from, len, data);
		}

		// -----------------------------------------------------------------------


	};
}
#endif /* __CTP_LINK_ESTIMATOR_H__ */
