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

#ifndef __CTP_NEIGHBOUR_TABLE_VALUE_H__
#define __CTP_NEIGHBOUR_TABLE_VALUE_H__

#include "algorithms/routing/ctp/ctp_types.h"

namespace wiselib {
	template<typename Radio_P>
	class CtpNeighbourTableValue {
	public:
		typedef Radio_P Radio;
		typedef typename Radio::node_id_t node_id_t;

		// ----------------------------------------------------------------------------------

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

		// -----------------------------------------------------------------

		CtpNeighbourTableValue() :
		lastseq(0), rcvcnt(0), failcnt(0), inage(0), inquality(0), eetx(0), data_success(0), data_total(0) {
		}

		// -----------------------------------------------------------------

		CtpNeighbourTableValue(uint8_t flags, uint8_t inage) :
		lastseq(0), rcvcnt(0), failcnt(0), inquality(0), eetx(0), data_success(0), data_total(0) {
			flags=flags;
			iange=inage;
		}

		// -----------------------------------------------------------------
	};

}
#endif /* __CTP_NEIGHBOUR_TABLE_VALUE_H__ */
