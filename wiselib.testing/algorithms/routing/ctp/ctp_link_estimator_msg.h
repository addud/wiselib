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

/*
 * When creating the message, we must first write the Ne field (set_ne()) and then set the payload (set_payload()).
 * For easier space management, there is no actual footer, just a longer header that includes
 * the neighbour entries as well. So the structure of the LE frame looks like this:
 *
 * ne | seqno | footer | payload
 *
 * The LE message structure is slightly different from the TinyOS implementation:
 * There is no reserved space for the footer, it can take all the space remaining from the header and payload
 * The NE (number of footer entries) field is not masked anymore, it takes up all 8 bits in the field.
 * Thus, the maximum number of footer entries is 255.
 * There an extra field dest, which is at the head of the frame and stores the destination node of the packet.
 * This field is used to differentiate between the routing beacons and the data frames.
 * The RE sends its beacons as broadcast messages, so the destination is BROADCAST_ADDRESS,
 * while the FE sends the messages as unicast, so the destination is usually the id of a single node.
 */

template<typename OsModel_P, typename Radio_P, size_t FOOTER_ENTRY_SIZE>
class CtpLinkEstimatorMsg {
public:
	typedef OsModel_P OsModel;
	typedef Radio_P Radio;
	typedef typename Radio::message_id_t message_id_t;
	typedef typename Radio::block_data_t block_data_t;
	typedef typename Radio::node_id_t node_id_t;

	// --------------------------------------------------------------------

	enum HeaderSize {
		HEADER_SIZE = sizeof(uint8_t) + sizeof(uint8_t)
	};

	// --------------------------------------------------------------------

	uint8_t ne() {
		return read<OsModel, block_data_t, uint8_t>(buffer + NE_POS);
	}

	// --------------------------------------------------------------------

	void set_ne(uint8_t ne) {
		write<OsModel, block_data_t, uint8_t>(buffer + NE_POS, ne);
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

	error_t add_neighbour_entry(uint8_t index, block_data_t* entry) {
		if (FOOTER_POS + (index + 1) * FOOTER_ENTRY_SIZE
				<= Radio::MAX_MESSAGE_LENGTH) {
			memcpy(buffer + FOOTER_POS + index * FOOTER_ENTRY_SIZE, entry,
					FOOTER_ENTRY_SIZE);
			return SUCCESS;
		}
		return ERR_UNSPEC;
	}

	// --------------------------------------------------------------------

	block_data_t* read_neighbour_entry(uint8_t index) {
		if (FOOTER_POS + (index + 1) * FOOTER_ENTRY_SIZE
				<= Radio::MAX_MESSAGE_LENGTH) {
			return buffer + index * FOOTER_ENTRY_SIZE;
		}
		return NULL;
	}

// --------------------------------------------------------------------

	inline block_data_t* payload() {
		return buffer + FOOTER_POS + ne() * FOOTER_ENTRY_SIZE;
	}

	// --------------------------------------------------------------------

	inline error_t set_payload(block_data_t *payload, size_t len) {
		if (FOOTER_POS + ne() * FOOTER_ENTRY_SIZE + len
				<= Radio::MAX_MESSAGE_LENGTH) {

			memcpy(buffer + FOOTER_POS + ne() * FOOTER_ENTRY_SIZE, payload,
					len);
			return SUCCESS;
		}
		return ERR_UNSPEC;
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
		NE_POS = 0, SEQNO_POS = NE_POS + sizeof(uint8_t), FOOTER_POS = SEQNO_POS
				+ sizeof(uint8_t)
	};

	// -----------------------------------------------------------------------

	block_data_t buffer[Radio::MAX_MESSAGE_LENGTH];

	// --------------------------------------------------------------------

};

}
#endif /* __CTP_LINK_ESTIMATOR_MSG_H__ */
