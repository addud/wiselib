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

#ifndef __CTP_FORWARDING_ENGINE_MSG_H__
#define __CTP_FORWARDING_ENGINE_MSG_H__

#include "algorithms/routing/ctp/ctp_types.h"

namespace wiselib {

template<typename OsModel_P, typename Radio_P>
class CtpForwardingEngineMsg {
public:
	typedef OsModel_P OsModel;
	typedef Radio_P Radio;
	typedef typename Radio::message_id_t message_id_t;
	typedef typename Radio::block_data_t block_data_t;
	typedef typename Radio::node_id_t node_id_t;

	// --------------------------------------------------------------------

	enum HeaderSize {
		HEADER_SIZE = sizeof(ctp_msg_options_t) + sizeof(uint8_t)
				+ sizeof(node_id_t) + sizeof(node_id_t) + sizeof(uint8_t)
	};

	// --------------------------------------------------------------------

	inline CtpForwardingEngineMsg(ctp_msg_options_t options, node_id_t parent,
			ctp_msg_etx_t etx) {
		set_options(options);
		set_parent(parent);
		set_etx(etx);
	}

	// --------------------------------------------------------------------

	inline CtpForwardingEngineMsg() {
	}

	// --------------------------------------------------------------------
	ctp_msg_options_t options() {
		return read<OsModel, block_data_t, ctp_msg_options_t>(
				buffer + OPTIONS_POS);
	}
	// --------------------------------------------------------------------
	void set_options(ctp_msg_options_t options) {
		write<OsModel, block_data_t, ctp_msg_options_t>(buffer + OPTIONS_POS,
				options);
	}
	// --------------------------------------------------------------------
	bool pull() {
		return (bool) ((read<OsModel, block_data_t, ctp_msg_options_t>(
				buffer + OPTIONS_POS)) & CtpMsgOptRoutingPull);
	}
	// --------------------------------------------------------------------
	void set_pull() {
		ctp_msg_options_t new_value = pull() | CtpMsgOptRoutingPull;
		write<OsModel, block_data_t, ctp_msg_options_t>(buffer + OPTIONS_POS,
				new_value);
	}
	// --------------------------------------------------------------------
	void clear_pull() {
		ctp_msg_options_t new_value = pull() & ~CtpMsgOptRoutingPull;
		write<OsModel, block_data_t, ctp_msg_options_t>(buffer + OPTIONS_POS,
				new_value);
	}
	// --------------------------------------------------------------------
	bool congestion() {
		return (bool) ((read<OsModel, block_data_t, ctp_msg_options_t>(
				buffer + OPTIONS_POS)) & CtpMsgOptCongestionNotification);
	}
	// --------------------------------------------------------------------
	void set_congestion() {
		ctp_msg_options_t new_value = pull() | CtpMsgOptCongestionNotification;
		write<OsModel, block_data_t, ctp_msg_options_t>(buffer + OPTIONS_POS,
				new_value);
	}
	// --------------------------------------------------------------------
	void clear_congestion() {
		ctp_msg_options_t new_value = pull() & ~CtpMsgOptCongestionNotification;
		write<OsModel, block_data_t, ctp_msg_options_t>(buffer + OPTIONS_POS,
				new_value);
	}
	// --------------------------------------------------------------------
	uint8_t thl() {
		return read<OsModel, block_data_t, uint8_t>(buffer + THL_POS);
	}
	// --------------------------------------------------------------------
	void set_thl(uint8_t thl) {
		write<OsModel, block_data_t, uint8_t>(buffer + THL_POS, thl);
	}
	// --------------------------------------------------------------------
	ctp_msg_etx_t etx() {
		return read<OsModel, block_data_t, ctp_msg_etx_t>(buffer + ETX_POS);
	}
	// --------------------------------------------------------------------
	void set_etx(ctp_msg_etx_t etx) {
		write<OsModel, block_data_t, ctp_msg_etx_t>(buffer + ETX_POS, etx);
	}
	// --------------------------------------------------------------------
	node_id_t origin() {
		return read<OsModel, block_data_t, node_id_t>(buffer + ORIGIN_POS);
	}
	// --------------------------------------------------------------------
	void set_origin(node_id_t origin) {
		write<OsModel, block_data_t, node_id_t>(buffer + ORIGIN_POS, origin);
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
	block_data_t* payload() {
		return buffer + DATA_POS;
	}
	// --------------------------------------------------------------------
	void set_payload(block_data_t *payload, size_t len) {
		memcpy(buffer + DATA_POS, payload, len);
	}

	// --------------------------------------------------------------------

private:

	enum data_positions {
		OPTIONS_POS = 0,
		THL_POS = OPTIONS_POS + sizeof(ctp_msg_options_t),
		ETX_POS = THL_POS + sizeof(uint8_t),
		ORIGIN_POS = ETX_POS + sizeof(node_id_t),
		SEQNO_POS = ORIGIN_POS + sizeof(node_id_t),
		DATA_POS = SEQNO_POS + sizeof(uint8_t)
	};

	// -----------------------------------------------------------------------

	enum CtpOptionsMasks {
		CtpMsgOptRoutingPull = 0x80, // Flag to request routing information
		CtpMsgOptCongestionNotification = 0x40 // Flag to signal congestion
	};

	// -----------------------------------------------------------------------

	block_data_t buffer[Radio::MAX_MESSAGE_LENGTH];

};

}
#endif /* __CTP_FORWARDING_ENGINE_MSG_H__ */
