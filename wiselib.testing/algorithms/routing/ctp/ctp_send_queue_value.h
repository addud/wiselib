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

#ifndef __CTP_SEND_QUEUE_VALUE_H__
#define __CTP_SEND_QUEUE_VALUE_H__

#include "algorithms/routing/ctp/ctp_types.h"

namespace wiselib {
	template<typename Radio_P>
	class CtpSendQueueValue {
	public:
		typedef Radio_P Radio;
		typedef typename Radio::block_data_t block_data_t;

		// ----------------------------------------------------------------------------------

		enum {
			MAX_METRIC = 0xFFFF
		};

		// ----------------------------------------------------------------------------------

		block_data_t* msg;
		size_t len;
		uint8_t client;
		uint8_t retries;

		// -----------------------------------------------------------------

		CtpSendQueueValue() {
		}

		// -----------------------------------------------------------------
	};

}
#endif /* __CTP_SEND_QUEUE_VALUE_H__ */
