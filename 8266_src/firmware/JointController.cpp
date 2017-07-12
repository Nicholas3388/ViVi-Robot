/*
	Copyright (c) 2015,
	- Kazuyuki TAKASE - https://github.com/junbowu
	- PLEN Project Company Inc. - https://plen.jp

	This software is released under the MIT License.
	(See also : http://opensource.org/licenses/mit-license.php)
*/
#include "Arduino.h"
#include <Wire.h>
#include <Servo.h>
#include <Ticker.h>
#include <Adafruit_PWMServoDriver.h>
#include <ESP8266WebServer.h>
#include "Pin.h"
#include "Profiler.h"
#include "System.h"
#include "JointController.h"
#include "ExternalFs.h"
#include "Motion.h"

Adafruit_PWMServoDriver pwm;
Servo GPIO12SERVO;
Servo GPIO14SERVO;
Servo EyeOut;
Ticker flipper;
Ticker flipper_second;
 WiFiServer tcp_server(23);
extern File fp_config;
/*!
	@note
	If you want to apply the firmware to PLEN1.4, set the macro to false.
*/
#define PLEN2_JOINTCONTROLLER_PWM_OUT_00_07_REGISTER OCR1C
#define PLEN2_JOINTCONTROLLER_PWM_OUT_08_15_REGISTER OCR1B
#define PLEN2_JOINTCONTROLLER_PWM_OUT_16_23_REGISTER OCR1A

volatile bool PLEN2::JointController::m_1cycle_finished = false;
int PLEN2::JointController::m_pwms[PLEN2::JointController::SUM];

//eyes control
int _enable;
int _eye_count_step;
uint8_t _eye_count;

namespace
{
	namespace Shared
	{
		using namespace PLEN2;

		PROGMEM const int m_SETTINGS_INITIAL[] =
		{
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,  -40, // [01] Left : Shoulder Pitch
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,  245, // [02] Left : Thigh Yaw
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,  470, // [03] Left : Shoulder Roll
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, -100, // [04] Left : Elbow Roll
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, -205, // [05] Left : Thigh Roll
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,   50, // [06] Left : Thigh Pitch
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,  445, // [07] Left : Knee Pitch
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,  245, // [08] Left : Foot Pitch
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,  -75, // [09] Left : Foot Roll
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, JointController::ANGLE_NEUTRAL,
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, JointController::ANGLE_NEUTRAL,
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, JointController::ANGLE_NEUTRAL,
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,   15, // [10] Right : Shoulder Pitch
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,  -70, // [11] Right : Thigh Yaw
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, -390, // [12] Right : Shoulder Roll
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,  250, // [13] Right : Elbow Roll
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,  195, // [14] Right : Thigh Roll
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, -105, // [15] Right : Thigh Pitch
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, -510, // [16] Right : Knee Pitch
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, -305, // [17] Right : Foot Pitch
			JointController::ANGLE_MIN, JointController::ANGLE_MAX,   60, // [18] Right : Foot Roll
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, JointController::ANGLE_NEUTRAL,
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, JointController::ANGLE_NEUTRAL,
			JointController::ANGLE_MIN, JointController::ANGLE_MAX, JointController::ANGLE_NEUTRAL
		};

		const int ERROR_LVALUE = -32768;
	}
}


void PLEN2::JointController::Init()
{
	pwm = Adafruit_PWMServoDriver();

    pinMode(Pin::PCA9685_ENABLE(), OUTPUT);
    digitalWrite(Pin::PCA9685_ENABLE(), LOW);
    pinMode(Pin::LED(),OUTPUT);
    digitalWrite(Pin::LED(),HIGH);
    GPIO12SERVO.attach(Pin::PWM_OUT_12());
    GPIO14SERVO.attach(Pin::PWM_OUT_14());
    _enable = true;
    _eye_count = 3;
  //  EyeOut.attach(Pin::LED_OUT(), 200, 15000);
//
//    pinMode(Pin::PCA9685_ENABLE(), OUTPUT);
//    digitalWrite(Pin::PCA9685_ENABLE(), LOW);

    // Initialize I2C
    Wire.begin(4, 5);

    // PWMServoDriver
    pwm.begin();
    pwm.setPWMFreq(PWM_FREQ());   // servos run at 300Hz updates

    delay(500);
    
	for (char joint_id = 0; joint_id < SUM; joint_id++)
	{
		m_SETTINGS[joint_id].MIN  = Shared::m_SETTINGS_INITIAL[joint_id * 3];
		m_SETTINGS[joint_id].MAX  = Shared::m_SETTINGS_INITIAL[joint_id * 3 + 1];
		m_SETTINGS[joint_id].HOME = Shared::m_SETTINGS_INITIAL[joint_id * 3 + 2];
		setAngle(joint_id, m_SETTINGS[joint_id].HOME);
	}
}
PLEN2::JointController::JointController()
{

}


void PLEN2::JointController::loadSettings()
{
	#if DEBUG
		volatile Utility::Profiler p(F("JointController::loadSettings()"));
	#endif

	unsigned char* filler = reinterpret_cast<unsigned char*>(m_SETTINGS);
	
	if (ExternalFs::readByte(INIT_FLAG_ADDRESS(), fp_config) != INIT_FLAG_VALUE())
	{
		ExternalFs::writeByte(INIT_FLAG_ADDRESS(), INIT_FLAG_VALUE(), fp_config);
        ExternalFs::write(SETTINGS_HEAD_ADDRESS(), sizeof(m_SETTINGS), filler, fp_config);
		System::debugSerial().println(F("reset config\n"));
	}
	else
	{
		ExternalFs::read(SETTINGS_HEAD_ADDRESS(), sizeof(m_SETTINGS), filler, fp_config);
		System::debugSerial().println(F("read config"));
	}

	for (char joint_id = 0; joint_id < SUM; joint_id++)
	{
		setAngle(joint_id, m_SETTINGS[joint_id].HOME);
	}

    flipper.attach_ms(Motion::Frame::UPDATE_INTERVAL_MS, PLEN2::JointController::updateAngle);
    flipper_second.attach(1, PLEN2::JointController::updateEyes);
}


void PLEN2::JointController::resetSettings()
{
	#if DEBUG
		volatile Utility::Profiler p(F("JointController::resetSettings()"));
	#endif
	
    ExternalFs::writeByte(INIT_FLAG_ADDRESS(), INIT_FLAG_VALUE(), fp_config);

	for (char joint_id = 0; joint_id < SUM; joint_id++)
	{
		m_SETTINGS[joint_id].MIN  = Shared::m_SETTINGS_INITIAL[joint_id * 3];
		m_SETTINGS[joint_id].MAX  = Shared::m_SETTINGS_INITIAL[joint_id * 3 + 1];
		m_SETTINGS[joint_id].HOME = Shared::m_SETTINGS_INITIAL[joint_id * 3 + 2];

		setAngle(joint_id, m_SETTINGS[joint_id].HOME);
	}
	
    ExternalFs::write(SETTINGS_HEAD_ADDRESS(), sizeof(m_SETTINGS), 
                    reinterpret_cast<const unsigned char*>(m_SETTINGS), fp_config);
}


const int& PLEN2::JointController::getMinAngle(unsigned char joint_id)
{
	#if DEBUG
		volatile Utility::Profiler p(F("JointController::getMinAngle()"));
	#endif

	if (joint_id >= SUM)
	{
		#if DEBUG
			System::debugSerial().print(F(">>> bad argment! : joint_id = "));
			System::debugSerial().println(static_cast<int>(joint_id));
		#endif

		return Shared::ERROR_LVALUE;
	}

	return m_SETTINGS[joint_id].MIN;
}


const int& PLEN2::JointController::getMaxAngle(unsigned char joint_id)
{
	#if DEBUG
		volatile Utility::Profiler p(F("JointController::getMaxAngle()"));
	#endif

	if (joint_id >= SUM)
	{
		#if DEBUG
			System::debugSerial().print(F(">>> bad argment! : joint_id = "));
			System::debugSerial().println(static_cast<int>(joint_id));
		#endif

		return Shared::ERROR_LVALUE;
	}

	return m_SETTINGS[joint_id].MAX;
}


const int& PLEN2::JointController::getHomeAngle(unsigned char joint_id)
{
	#if DEBUG
		volatile Utility::Profiler p(F("JointController::getHomeAngle()"));
	#endif

	if (joint_id >= SUM)
	{
		#if DEBUG
			System::debugSerial().print(F(">>> bad argment! : joint_id = "));
			System::debugSerial().println(static_cast<int>(joint_id));
		#endif

		return Shared::ERROR_LVALUE;
	}

	return m_SETTINGS[joint_id].HOME;
}


bool PLEN2::JointController::setMinAngle(unsigned char joint_id, int angle)
{
	#if DEBUG
		volatile Utility::Profiler p(F("JointController::setMinAngle()"));
	#endif

	if (joint_id >= SUM)
	{
		#if DEBUG
			System::debugSerial().print(F(">>> bad argment! : joint_id = "));
			System::debugSerial().println(static_cast<int>(joint_id));
		#endif

		return false;
	}

	if (   (angle >= m_SETTINGS[joint_id].MAX)
		|| (angle <  ANGLE_MIN) )
	{
		#if DEBUG
			System::debugSerial().print(F(">>> bad argment! : angle = "));
			System::debugSerial().println(angle);
		#endif

		return false;
	}


	m_SETTINGS[joint_id].MIN = angle;

	unsigned char* filler = reinterpret_cast<unsigned char*>(&(m_SETTINGS[joint_id].MIN));
	int address_offset    = reinterpret_cast<int>(filler) - reinterpret_cast<int>(m_SETTINGS);

	#if DEBUG
		System::debugSerial().print(F(">>> address_offset : "));
		System::debugSerial().println(address_offset);
	#endif

	ExternalFs::write(SETTINGS_HEAD_ADDRESS() + address_offset, sizeof(m_SETTINGS[joint_id].MIN), filler, fp_config);

	return true;
}


bool PLEN2::JointController::setMaxAngle(unsigned char joint_id, int angle)
{
	#if DEBUG
		volatile Utility::Profiler p(F("JointController::setMaxAngle()"));
	#endif

	if (joint_id >= SUM)
	{
		#if DEBUG
			System::debugSerial().print(F(">>> bad argment! : joint_id = "));
			System::debugSerial().println(static_cast<int>(joint_id));
		#endif

		return false;
	}

	if (   (angle <= m_SETTINGS[joint_id].MIN)
		|| (angle >  ANGLE_MAX) )
	{
		#if DEBUG
			System::debugSerial().print(F(">>> bad argment! : angle = "));
			System::debugSerial().println(angle);
		#endif

		return false;
	}


	m_SETTINGS[joint_id].MAX = angle;

	unsigned char* filler = reinterpret_cast<unsigned char*>(&(m_SETTINGS[joint_id].MAX));
	int address_offset    = reinterpret_cast<int>(filler) - reinterpret_cast<int>(m_SETTINGS);

	#if DEBUG
		System::debugSerial().print(F(">>> address_offset : "));
		System::debugSerial().println(address_offset);
	#endif

	ExternalFs::write(SETTINGS_HEAD_ADDRESS() + address_offset, sizeof(m_SETTINGS[joint_id].MAX), filler, fp_config);

	return true;
}


bool PLEN2::JointController::setHomeAngle(unsigned char joint_id, int angle)
{
	#if DEBUG
		volatile Utility::Profiler p(F("JointController::setHomeAngle()"));
	#endif

	if (joint_id >= SUM)
	{
		#if DEBUG
			System::debugSerial().print(F(">>> bad argment! : joint_id = "));
			System::debugSerial().println(static_cast<int>(joint_id));
		#endif

		return false;
	}

	if (   (angle < m_SETTINGS[joint_id].MIN)
		|| (angle > m_SETTINGS[joint_id].MAX) )
	{
		#if DEBUG
			System::debugSerial().print(F(">>> bad argment! : angle = "));
			System::debugSerial().println(angle);
		#endif

		return false;
	}


	m_SETTINGS[joint_id].HOME = angle;

	unsigned char* filler = reinterpret_cast<unsigned char*>(&(m_SETTINGS[joint_id].HOME));
	int address_offset    = reinterpret_cast<int>(filler) - reinterpret_cast<int>(m_SETTINGS);

	#if DEBUG
		System::debugSerial().print(F(">>> address_offset : "));
		System::debugSerial().println(address_offset);
	#endif

	ExternalFs::write(SETTINGS_HEAD_ADDRESS() + address_offset, sizeof(m_SETTINGS[joint_id].HOME), filler, fp_config);

	return true;
}

//设置角度
bool PLEN2::JointController::setAngle(unsigned char joint_id, int angle)
{
	#if DEBUG_HARD
		volatile Utility::Profiler p(F("JointController::setAngle()"));
	#endif

	if (joint_id >= SUM)
	{
		#if DEBUG_HARD
			System::debugSerial().print(F(">>> bad argment! : joint_id = "));
			System::debugSerial().println(static_cast<int>(joint_id));
		#endif

		return false;
	}


	angle = constrain(angle, m_SETTINGS[joint_id].MIN, m_SETTINGS[joint_id].MAX);
	if(joint_id  == 0 || joint_id == 12)
	{
		#if CLOCK_WISE
			m_pwms[joint_id] = 90 + angle / 10;
		#else
			m_pwms[joint_id] = 90 - angle / 10;
		#endif
	}
	else
	{
		m_pwms[joint_id] = map(
			angle,
			PLEN2::JointController::ANGLE_MIN, PLEN2::JointController::ANGLE_MAX,

			#if CLOCK_WISE
				PLEN2::JointController::PWM_MIN(), PLEN2::JointController::PWM_MAX()
			#else
				PLEN2::JointController::PWM_MAX(), PLEN2::JointController::PWM_MIN()
			#endif
		);
	}
#if DEBUG_LESS
	System::debugSerial().print(F(": joint_id = "));
	System::debugSerial().print(static_cast<int>(joint_id));
	System::debugSerial().print(F(": angle = "));
	System::debugSerial().print(static_cast<int>(angle));
	System::debugSerial().print(F(": pwm = "));
	System::debugSerial().print(static_cast<int>(m_pwms[joint_id]));
#endif
	return true;
}

//设置角度差
bool PLEN2::JointController::setAngleDiff(unsigned char joint_id, int angle_diff)
{
	#if DEBUG_HARD
		volatile Utility::Profiler p(F("JointController::setAngleDiff()"));
	#endif

	if (joint_id >= SUM)
	{
		#if DEBUG_HARD
			System::debugSerial().print(F(">>> bad argment! : joint_id = "));
			System::debugSerial().println(static_cast<int>(joint_id));
		#endif

		return false;
	}


	int angle = constrain(
		angle_diff + m_SETTINGS[joint_id].HOME,
		m_SETTINGS[joint_id].MIN, m_SETTINGS[joint_id].MAX
	);

	if(joint_id  == 0 || joint_id == 12)
	{
		#if CLOCK_WISE
			m_pwms[joint_id] = 90 + angle / 10;
		#else
			m_pwms[joint_id] = 90 - angle / 10;
		#endif
	}
	else
	{
		m_pwms[joint_id] = map(
			angle,
			PLEN2::JointController::ANGLE_MIN, PLEN2::JointController::ANGLE_MAX,

			#if CLOCK_WISE
				PLEN2::JointController::PWM_MIN(), PLEN2::JointController::PWM_MAX()
			#else
				PLEN2::JointController::PWM_MAX(), PLEN2::JointController::PWM_MIN()
			#endif
		);
	}

	return true;
}


void PLEN2::JointController::dump()
{
	#if DEBUG
		volatile Utility::Profiler p(F("JointController::dump()"));
	#endif
	System::outputSerial().println(F("["));

	for (char joint_id = 0; joint_id < SUM; joint_id++)
	{
		System::outputSerial().println(F("\t{"));

		System::outputSerial().print(F("\t\t\"max\": "));
		System::outputSerial().print(m_SETTINGS[joint_id].MAX);
		System::outputSerial().println(F(","));

		System::outputSerial().print(F("\t\t\"min\": "));
		System::outputSerial().print(m_SETTINGS[joint_id].MIN);
		System::outputSerial().println(F(","));

		System::outputSerial().print(F("\t\t\"home\": "));
		System::outputSerial().println(m_SETTINGS[joint_id].HOME);
    tcp_server.write(m_SETTINGS[joint_id].HOME);

		System::outputSerial().print(F("\t}"));

		if (joint_id != (SUM - 1))
		{
			System::outputSerial().println(F(","));
		}
		else
		{
			System::outputSerial().println();
		}
	}

	System::outputSerial().println(F("]"));
	System::outputSerial().println(F("\r\n"));
}


/*
	@brief Timer 1 overflow interruption vector

	The interruption vector runs at the moment TCNT1 overflowed.
	In the firmware, 16[MHz] clock source is prescaled by 64, and using 10bit mode,
	so interruption interval is (16,000,000 / (64 * 1,024))^-1 * 1,000 = 4.096[msec].

	The value is too smaller than servo's PWM acceptable interval,
	so the firmware can control 24 servos by outputting PWM once in 8 times
	and changing output line at each interruption timing.

	@attention
	Please add volatile prefix to all editable instance of the vector,
	for countermeasure of optimization.

	The MCU outputs PWM with double-buffering so joint selection should look ahead next joint.
	If fail to do the procedure, controlling plural servos is not to be you intended.
*/
/*
 * TODO: update angle
 */
const unsigned char servo_map[PLEN2::JointController::SUM] = {16, 7, 6, 5, 4, 3, 2, 1, 0, 18, 19, 20, 17, 8, 9, 10, 11, 12, 13, 14, 15, 21, 22, 23};
void PLEN2::JointController::updateAngle()
{
    for (int joint_id = 0; joint_id < SUM; joint_id++)
    {
        if (servo_map[joint_id] < 16)
	    {
	        pwm.setPWM(servo_map[joint_id], 0, m_pwms[joint_id]);
	    }
        else if (servo_map[joint_id] == 16)
        {
           GPIO12SERVO.write(m_pwms[joint_id]);
           
        }
        else if (servo_map[joint_id] == 17)
        {
            GPIO14SERVO.write(m_pwms[joint_id]);
        }
    }
	PLEN2::JointController::m_1cycle_finished = true;
}

void PLEN2::JointController::updateEyes()
{
    unsigned int led_pwm = 0;


        if (PLEN2::System::tcp_connected()||PLEN2::System::SystemSerial().available())
        {
            //connected
            if (_enable&&_eye_count>0)
            {
             digitalWrite(Pin::LED(),LOW);
             _eye_count--;
             _enable=false;
            }else if(_eye_count>0)
            {
              digitalWrite(Pin::LED(),HIGH);
              _enable=true;
              }

        }
        else if (!PLEN2::System::tcp_connected()&&!PLEN2::System::SystemSerial().available())
        {
        	_eye_count=3;
        	_enable=true;
        }
      
}

