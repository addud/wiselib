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

#ifndef __CTP_RANDOM_NUMBER_H__
#define __CTP_RANDOM_NUMBER_H__

#include <limits.h>

namespace wiselib {

template<typename OsModel_P, typename Clock_P = typename OsModel_P::Clock,
		typename Debug_P = typename OsModel_P::Debug>
class CtpRandomNumber {
public:
	typedef OsModel_P OsModel;
	typedef Clock_P Clock;
	typedef Debug_P Debug;

	typedef CtpRandomNumber<OsModel, Clock, Debug> self_type;
	typedef self_type* self_pointer_t;

	typedef typename Clock::time_t time_t;

	typedef uint32_t value_t;

	// -----------------------------------------------------------------------

	enum ErrorCodes {
		SUCCESS = OsModel::SUCCESS,
		ERR_UNSPEC = OsModel::ERR_UNSPEC,
		ERR_NOTIMPL = OsModel::ERR_NOTIMPL,
		ERR_BUSY = OsModel::ERR_BUSY
	};

	// -----------------------------------------------------------------------

	enum Restrictions {
		RANDOM_MAX = ULONG_MAX
	};

	// -----------------------------------------------------------------------

	CtpRandomNumber() {
	}

	// -----------------------------------------------------------------------

	~CtpRandomNumber() {
	}

	// -----------------------------------------------------------------------
	int init(Debug& debug, Clock& clock) {
		debug_ = &debug;
		clock_ = &clock;

		return SUCCESS;
	}

	// -----------------------------------------------------------------------

	void srand(uint32_t dummy) {

	}

	// -----------------------------------------------------------------------

	value_t rand(value_t max_value) {
			return clock().milliseconds(clock().time()) % max_value;
	}

	// -----------------------------------------------------------------------

private:

	typename Debug::self_pointer_t debug_;
	typename Clock::self_pointer_t clock_;

	// -----------------------------------------------------------------------

	Debug& debug() {
		return *debug_;
	}

	Clock& clock() {
		return *clock_;
	}

	// -----------------------------------------------------------------------

};
}
#endif /* __CTP_RANDOM_NUMBER_H__ */
