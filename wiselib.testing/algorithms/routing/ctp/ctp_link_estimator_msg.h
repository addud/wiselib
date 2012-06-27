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

#ifndef __CTP_LINK_ESTIMATOR_MSG_H__
#define __CTP_LINK_ESTIMATOR_MSG_H__

#include "algorithms/routing/ctp/ctp_types.h"

namespace wiselib {

	template<typename OsModel_P, typename Radio_P, size_t FOOTER_ENTRY_SIZE>
	class CtpLinkEstimatorMsg {
	public:
		typedef OsModel_P OsModel;
		typedef Radio_P Radio;
		typedef typename Radio::message_id_t message_id_t;
		typedef typename Radio::block_data_t block_data_t;
		typedef typename Radio::node_id_t node_id_t;

		// --------------------------------------------------------------------

		inline CtpLinkEstimatorMsg(){
			init();
		}

		// --------------------------------------------------------------------

		void init() {
			curr_pos_ = footer_pos_ = DATA_POS;
			buffer_size_ =DATA_POS;
		}

		// --------------------------------------------------------------------

		uint8_t ne() {
			return read<OsModel, block_data_t, uint8_t>(buffer + NE_POS) & CtpMsgNeMask;
		}

		// --------------------------------------------------------------------

		void set_ne(uint8_t ne) {
			ne = ne & CtpMsgNeMask;
			write<OsModel, block_data_t, uint8_t>(buffer + NE_POS, ne);

			if (buffer_size() + ne * FOOTER_ENTRY_SIZE <= Radio::MAX_MESSAGE_LENGTH) {
				buffer_size_ += ne * FOOTER_ENTRY_SIZE;
			}
		}

		// --------------------------------------------------------------------

		uint8_t seqno() {
			return read<OsModel, block_data_t, uint8_t>(buffer + SEQNO_POS);
		}

		// --------------------------------------------------------------------

		void set_seqno(uint8_t seqno) {
			write<OsModel, block_data_t, uint8_t>(buffer + SEQNO_POS, seqno);
		}

		// --------------------------------------------------------------------

		error_t add_neighbour_entry(block_data_t* entry) {
			if (curr_pos_ + FOOTER_ENTRY_SIZE <= Radio::MAX_MESSAGE_LENGTH) {
				memcpy(buffer+curr_pos_, entry, FOOTER_ENTRY_SIZE);
				curr_pos_+=FOOTER_ENTRY_SIZE;
				return SUCCESS;
			}
			return  ERR_UNSPEC;
		}

		// --------------------------------------------------------------------

		block_data_t* read_neighbour_entry() {
			if (footer_pos_ == DATA_POS) {
				footer_pos_ = Radio::MAX_MESSAGE_LENGTH - ne() * FOOTER_ENTRY_SIZE;
			}

			if (curr_pos_+FOOTER_ENTRY_SIZE <= Radio::MAX_MESSAGE_LENGTH) {	
				block_data_t* entry;
				memcpy(entry, curr_pos_, FOOTER_ENTRY_SIZE);
				return entry;
			} 
			return NULL;
		}

		// --------------------------------------------------------------------

		error_t set_data(block_data_t* data, size_t size) {

			if (buffer_size() + size <= Radio::MAX_MESSAGE_LENGTH) {
				memcpy(buffer+DATA_POS, data, size);
				buffer_size_ += size;
				curr_pos_ = footer_pos_ = DATA_POS+ size;
				return SUCCESS;
			}
			return ERR_UNSPEC;
		}

		// --------------------------------------------------------------------

		block_data_t* data() {
			block_data_t* data;
			memcpy(data, DATA_POS, buffer_size()-DATA_POS - ne()*FOOTER_ENTRY_SIZE);
			return data;
		}


		// --------------------------------------------------------------------

		size_t buffer_size() {
			return buffer_size_;
		}

		// --------------------------------------------------------------------

	private:

		enum ErrorCodes {
			SUCCESS = OsModel::SUCCESS,
			ERR_UNSPEC = OsModel::ERR_UNSPEC,
			ERR_NOTIMPL = OsModel::ERR_NOTIMPL,
			ERR_BUSY = OsModel::ERR_BUSY
		};

		// --------------------------------------------------------------------

		enum data_positions {
			NE_POS = 0,
			SEQNO_POS = NE_POS + 1,
			DATA_POS = SEQNO_POS + 1
		};

		// -----------------------------------------------------------------------

		enum CtpLeMasks {
			CtpMsgNeMask = 0x0F, // Flag to request routing information
		};

		// -----------------------------------------------------------------------

		block_data_t buffer[Radio::MAX_MESSAGE_LENGTH];

		uint8_t footer_pos_, curr_pos_;

		size_t buffer_size_;

		// --------------------------------------------------------------------

	};

}
#endif /* __CTP_LINK_ESTIMATOR_MSG_H__ */
