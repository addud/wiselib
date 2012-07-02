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

#ifndef CTP_TYPES_H_
#define CTP_TYPES_H_

#include "util/serialization/simple_types.h"

#define DEBUG_NODES				{0,1,2,3,4,5,6,7}
#define DEBUG_NODES_NR 			8
#define ROOT_NODES_NR 			1
#define ROOT_NODES 					{6}

namespace wiselib
{

   typedef uint8_t ctp_msg_options_t;
   typedef uint16_t ctp_msg_etx_t;

//   enum {
//       SUCCESS = 1,
//       FAIL = 2,
//       EBUSY = 3,
//       ERETRY = 4,
//       ECANCEL = 5,
//       EOFF = 6,
//       ESIZE = 7
//   } error_t;

   typedef int error_t;

}


#endif /* CTP_TYPES_H_ */
