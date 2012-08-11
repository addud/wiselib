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

#ifndef __CTP_ROUTING_ENGINE_MSG_H__
#define __CTP_ROUTING_ENGINE_MSG_H__

#include "algorithms/routing/ctp/ctp_types.h"

namespace wiselib {

	template<typename OsModel_P, typename Radio_P>
	class CtpRoutingEngineMsg {
	public:
		typedef OsModel_P OsModel;
		typedef Radio_P Radio;
		typedef typename Radio::message_id_t message_id_t;
		typedef typename Radio::block_data_t block_data_t;
		typedef typename Radio::node_id_t node_id_t;

		// --------------------------------------------------------------------

		enum HeaderSize {
			HEADER_SIZE = sizeof(message_id_t) +sizeof(ctp_msg_options_t)
			+ sizeof(node_id_t) + sizeof(ctp_etx_t)
		};

		// --------------------------------------------------------------------

		inline CtpRoutingEngineMsg() {
		}

		inline CtpRoutingEngineMsg(message_id_t id) {
			set_msg_id(id);
		}

		// --------------------------------------------------------------------
		message_id_t msg_id() {
			return read<OsModel, block_data_t, message_id_t>(
				buffer + ID_POS);
		}
		// --------------------------------------------------------------------
		void set_msg_id(message_id_t id) {
			write<OsModel, block_data_t, message_id_t>(buffer + ID_POS,
				id);
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
		node_id_t parent() {
			return read<OsModel, block_data_t, node_id_t>(buffer + PARENT_POS);
		}
		// --------------------------------------------------------------------
		void set_parent(node_id_t parent) {
			write<OsModel, block_data_t, node_id_t>(buffer + PARENT_POS, parent);
		}
		// --------------------------------------------------------------------
		ctp_etx_t etx() {
			return read<OsModel, block_data_t, ctp_etx_t>(buffer + ETX_POS);
		}
		// --------------------------------------------------------------------
		void set_etx(ctp_etx_t etx) {
			write<OsModel, block_data_t, ctp_etx_t>(buffer + ETX_POS, etx);
		}

	private:
		enum data_positions {
			ID_POS=0,
			OPTIONS_POS = ID_POS+sizeof(message_id_t),
			PARENT_POS = OPTIONS_POS + sizeof(ctp_msg_options_t),
			ETX_POS = PARENT_POS + sizeof(node_id_t)
		};

		// -----------------------------------------------------------------------

		enum CtpOptionsMasks {
			CtpMsgOptRoutingPull = 0x80, // Flag to request routing information
			CtpMsgOptCongestionNotification = 0x40
			// Flag to signal congestion
		};

		// -----------------------------------------------------------------------

		block_data_t buffer[HEADER_SIZE];

	};

}
#endif /* __CTP_ROUTING_ENGINE_MSG_H__ */
