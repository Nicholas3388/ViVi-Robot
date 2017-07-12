/*!
	@file      Pin.h
	@brief     Management namespace of pin mapping.
	@author    Kazuyuki TAKASE
	@copyright The MIT License - http://opensource.org/licenses/mit-license.php
*/

#pragma once

#ifndef PLEN2_PIN_H
#define PLEN2_PIN_H

namespace PLEN2
{
	/*!
		@brief Management namespace of pin mapping

		Please give the standard Arduino libraries the methods returning values.
		The methods are evaluated at compile time, so there is no overhead at runtime.

		@note
		It helps your understanding that to refer the PLEN2's circuit and schematic.
		-> https://github.com/plenproject/plen__baseboard

		@sa
		Arduino Micro's pin mapping -> http://arduino.cc/en/Hacking/PinMapping32u4
	 */
	namespace Pin
	{

		//! @brief Output of PWM, for servo 12
		inline static const int PWM_OUT_12()          { return 12; }

		//! @brief Output of PWM, for servo 14
		inline static const int PWM_OUT_14()          { return 14; }
        //! @brief Output of PCA9685
        inline static const int PCA9685_ENABLE()      { return 13; }
		//! @brief Output of LED TODO TBD
		inline static const int LED_OUT()             { return 10; }
    inline static const int LED()                 {return 16;}
	}
}

#endif // PLEN2_PIN_H
