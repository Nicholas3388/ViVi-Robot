/*!
	@file      ExternalFs.h
	@brief     Management class of external EEPROM.
	@author    Kazuyuki TAKASE
	@copyright The MIT License - http://opensource.org/licenses/mit-license.php
*/

#pragma once

#ifndef PLEN2_EXTERNAL_FS_H
#define PLEN2_EXTERNAL_FS_H
#include "FS.h"


#define MOTION_FILE  "/motion.bin"
#define MOTION_FILE_SIZE 0x200000L

#define CONFIG_FILE  "/joint_cfg.bin"
#define CONFIG_FILE_SIZE 0x1000L

#define SYSCFG_FILE  "/sys_cfg.bin"
#define SYSCFG_FILE_SIZE 0x1000L

#define BUF_SIZE    (1024)

namespace PLEN2
{
	class ExternalFs;
}

/*!
	@brief Management class of external EEPROM

	@note
	24FC1025 supports to access 128 bytes at once, but Arduino's I2C library is not supported the method
	because the library's buffer size is 32 bytes.
	<br><br>
	Please pay attention to it is including bytes of targeted area address (= 2 bytes).
	Accurate data size are 30 bytes that you can write.
	(This is the reason that there are differences between CHUNK_SIZE() and SLOT_SIZE().)
*/
class PLEN2::ExternalFs
{
private:

	//! @brief Size of external SPIFs (bytes)
	inline static const long SIZE()          { return MOTION_FILE_SIZE; }

public:
	//! @brief Chunk size of external EEPROM (bytes)
	inline static const int CHUNK_SIZE() { return 32; }

	//! @brief Slot size of external EEPROM (bytes)
	inline static const int SLOT_SIZE()  { return 30; }

	//! @brief Beginning value of slots
	inline static const int SLOT_BEGIN() { return 0; }

	//! @brief End value of slots
	inline static const int SLOT_END()   { return SIZE() / CHUNK_SIZE(); }

	/*!
		@brief Constructor
	*/
	static void init();
    static void de_init();
    static char read(unsigned int start_addr, unsigned int size, unsigned char data[], File fp);
    static char write(unsigned int start_addr, unsigned int size, const unsigned char data[], File fp);
    static unsigned char readByte(unsigned int start_addr, File fp);
    static char writeByte(unsigned int start_addr, const unsigned char data, File fp);

    /*!
		@brief Read a slot of external EEPROM

		@param [in]  slot      Please set slot number you want to read.
		@param [out] data[]    Please set buffer to store reading data.
		@param [in]  read_size Please set buffer size.

		@return Result
		@retval !0 Succeeded. (Generally, the value equals **read-size**.)
		@retval -1 Failed.
	*/
	static char readSlot(unsigned int slot, unsigned char data[], unsigned char read_size, File fp);

	/*!
		@brief Write a slot of external EEPROM

		@param [in] slot       Please set slot number you want to write.
		@param [in] data[]     Please set buffer that stored writing data.
		@param [in] write_size Please set buffer size.

		@return Result
		@retval 0  Succeeded.
		@retval -1 Argument error. (**write_size** is bigger than slot size.)
		@retval 1  Sending-buffer overflow.
		@retval 2  Received NACK after sending slave address.
		@retval 3  Received NACK after sending data bytes.
		@retval 4  Other errors were raised.

		@attention
		Writing external EEPROM requires time. (Typically using 3[msec].)
		In the implementation, 5[msec] delay is inserted at end of the method.
	*/
	static char writeSlot(unsigned int slot, const unsigned char data[], unsigned char write_size, File fp);
};

#endif // PLEN2_EXTERNAL_EEPROM_H
