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

#ifndef CTP_TESTING_H_
#define CTP_TESTING_H_

#include "util/serialization/simple_types.h"
#include <limits.h>

//uncomment for debugging mode
#define CTP_DEBUGGING

//uncomment to enable hardcoded link quality gradients
#define DEBUG_ETX

//Uncomment to enable general RE debug messages
//#define FORWARDING_ENGINE_DEBUG

//Uncomment to enable general RE debug messages
//#define ROUTING_ENGINE_DEBUG

//Uncomment to enable general LE debug messages
//#define LINK_ESTIMATOR_DEBUG

#define NODES_NR			4
//#define NODES				{410,411,412,413,414,415,416,417} //tubs isense ids
//#define NODES				{418,419,420,421,422,424,426,427} // alternate tubs isense ids
#define NODES				{0,1,2,3} //Shawn ids


/* All these numbers represent indexes of the node IDs in the NODES list. */

//#define LINKS_NR			8
//#define LINKS 			{{0,1,1},{1,2,1},{2,3,1},{3,4,1},{4,5,1},{5,6,1},{6,7,1},{7,0,1}}

#define LINKS_NR			4
#define LINKS 				{{0,1,20},{0,2,19},{3,1,20},{3,2,40}}

#define SENDER_NODES_NR		1
#define SENDER_NODES		{3}

//Nodes acting as roots/sinks
#define ROOT_NODES_NR 		1
#define ROOT_NODES 			{0}

//Node indexes we want to print debug messages on
//#define DEBUG_NODES			{0,1,2,3,4,5,6,7}
//#define DEBUG_NODES_NR 		8
#define DEBUG_NODES		{3}
#define DEBUG_NODES_NR 	1

namespace wiselib {

typedef struct {
	uint16_t n1;
	uint16_t n2;
	ctp_etx_t etx;
} links_t;

const uint16_t nodes[NODES_NR] = NODES;
const uint16_t sender_nodes[SENDER_NODES_NR] = SENDER_NODES;
const uint16_t root_nodes[ROOT_NODES_NR] = ROOT_NODES;
const uint16_t debug_nodes[DEBUG_NODES_NR] = DEBUG_NODES;

links_t links[LINKS_NR] = LINKS;

links_t* getLink(uint16_t n1, uint16_t n2) {

	int i;

	for (i = 0; i < LINKS_NR; i++) {
		if (((n1 == nodes[links[i].n1])
				&& (n2 == nodes[links[i].n2]))
				|| ((n1 == nodes[links[i].n2])
						&& (n2 == nodes[links[i].n1]))) {
			return &(links[i]);
		}
	}

	return NULL;
}

bool areConnected(uint16_t n1, uint16_t n2) {
	links_t *link = getLink(n1,n2);

	if (link!=NULL) {
		if (link->etx < USHRT_MAX) {
			return true;
		}
	}

	return false;
}

}

#endif /* CTP_TYPES_H_ */
