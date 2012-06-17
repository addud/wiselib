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
#ifndef __ALGORITHMS_ROUTING_DSR_TYPES_H__
#define __ALGORITHMS_ROUTING_DSR_TYPES_H__

#include "algorithms/routing/ctp/ctp_types.h"

namespace wiselib {
template<typename Radio_P>
class CtpRoutingTableValue {
public:
	typedef Radio_P Radio;
	typedef typename Radio::node_id_t node_id_t;

	enum {
		MAX_METRIC = 0xFFFF
	};

	node_id_t parent;
	ctp_msg_etx_t etx;
	bool haveHeard;
	bool congested;

// -----------------------------------------------------------------
	CtpRoutingTableValue() :
			parent(Radio_P::NULL_NODE_ID), etx(MAX_METRIC), haveHeard(false), congested(
					false) {
	}
// -----------------------------------------------------------------
};

}
#endif
