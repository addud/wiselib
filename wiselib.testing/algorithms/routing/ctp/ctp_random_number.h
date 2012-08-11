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

   /**
    * \brief An implementation of the  \ref random_number_concept "Random Number Concept" based on the Basic Linear Congruential Generator algorithm
    *
    *  \ingroup random_number_concept
    *
    * An implementation of the  \ref random_number_concept "Random Number Concept" based on the Basic Linear Congruential Generator algorithm


	The heart of LCG is the following formula

X(i+1) = (a * X(i) + c) mod M


NOTE: a and c are selected for no particular properties
and I have not run statistical tests on this number gernerator
it is was written for demonstration purposes only.

Since this is mod 2^32 and our data type is 32 bits long
there is no need for the MOD operator.

    */

	template<typename OsModel_P, typename Radio_P = typename OsModel_P::Radio, typename Clock_P = typename OsModel_P::Clock,
		typename Debug_P = typename OsModel_P::Debug>
	class CtpRandomNumber {
	public:
		typedef OsModel_P OsModel;
		typedef Radio_P Radio;
		typedef Clock_P Clock;
		typedef Debug_P Debug;

		typedef CtpRandomNumber<OsModel, Radio, Clock, Debug> self_type;
		typedef self_type* self_pointer_t;

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

		CtpRandomNumber(unsigned long iSeed) {
			srand(iSeed);
		}

		// -----------------------------------------------------------------------

		~CtpRandomNumber() {
		}

		// -----------------------------------------------------------------------
		int init(Radio& radio, Debug& debug, Clock& clock) {
			radio_ = &radio;
			debug_ = &debug;
			clock_ = &clock;

			//Chose a default seed
			srand(clock_->milliseconds(clock_->time()* (3 * radio_->id() + 2)) );

			return SUCCESS;
		}

		// -----------------------------------------------------------------------

		void srand(unsigned long iSeed) {
			iCurrent_ = iSeed;
		}

		// -----------------------------------------------------------------------

		unsigned long rand(unsigned long max_value) {
			return randLong(max_value);
		}

		// -----------------------------------------------------------------------

		/* Functions for generating different types of random data */ 


	      ///@name Other Data Types
      ///@{
		unsigned long randLong(unsigned long limit = ULONG_MAX)
		{
			unsigned long iOutput;
			unsigned long iTemp;
			int i;
			//I only want to take the top two bits
			//This will shorten our period to (2^32)/16=268,435,456
			//Which seems like plenty to me.
			for(i=0; i<16; i++)
			{
				iCurrent_ = (MACRO_A * iCurrent_ + 1);
				iTemp = iCurrent_ >> 30;
				iOutput = iOutput << 2;
				iOutput = iOutput + iTemp;
			}

			return (limit>0) ? iOutput % limit : iOutput;
		}

		// -----------------------------------------------------------------------

		unsigned short int randShort(unsigned short limit = USHRT_MAX)
		{
			unsigned short int iOutput;
			unsigned long iTemp;
			int i;
			//No need to limit ourselves...
			for(i=0; i<8; i++)
			{
				iCurrent_ = (MACRO_A * iCurrent_ + 1);
				iTemp = iCurrent_ >> 30;
				iOutput = iOutput << 2;
				iOutput = iOutput + (short int)iTemp;
			}
			return (limit>0) ? iOutput % limit : iOutput;
		}

		// -----------------------------------------------------------------------

		unsigned char randChar(unsigned char limit = CHAR_MAX)
		{
			unsigned char cOutput;
			unsigned long iTemp;
			int i;
			for(i=0; i<4; i++)
			{
				iCurrent_ = (MACRO_A * iCurrent_ + 1);
				iTemp = iCurrent_ >> 30;
				cOutput = cOutput << 2;
				cOutput = cOutput + (char)iTemp;
			}
			return (limit>0) ? cOutput % limit : cOutput;
		}

		// -----------------------------------------------------------------------

		bool randBit()
		{
			iCurrent_ = (MACRO_A * iCurrent_ + 1);
			return (iCurrent_ >> 31) == 0;	 
		}
		///@}
		// -----------------------------------------------------------------------

	private:

		typename Radio::self_pointer_t radio_;
		typename Debug::self_pointer_t debug_;
		typename Clock::self_pointer_t clock_;

		static const unsigned long MACRO_A = 3039177861UL;

		unsigned long iCurrent_;

		// -----------------------------------------------------------------------

		Debug& debug() {
			return *debug_;
		}

		Clock& clock() {
			return *clock_;
		}

		Radio& radio() {
			return *radio_;
		}

		// -----------------------------------------------------------------------
	};
}
#endif /* __CTP_RANDOM_NUMBER_H__ */
