/*
	Copyright (c) 2015,
	- Kazuyuki TAKASE - https://github.com/junbowu
	- PLEN Project Company Inc. - https://plen.jp

	This software is released under the MIT License.
	(See also : http://opensource.org/licenses/mit-license.php)
*/
#include "Arduino.h"
#include <Wire.h>

#include "ExternalFs.h"
#include "System.h"
#include "Profiler.h"

File fp_motion;
File fp_config;
File fp_syscfg;

void PLEN2::ExternalFs::init()
{
	File fp;
    unsigned int i;
	unsigned int start_addr;
	unsigned char buf[BUF_SIZE] = {1};
    bool result = SPIFFS.begin();
#if DEBUG
    System::debugSerial().println("SPIFFS opened: " + result);
#endif

    if (!SPIFFS.exists(MOTION_FILE) ||
        !SPIFFS.exists(CONFIG_FILE) ||
        !SPIFFS.exists(SYSCFG_FILE))
    {
    	System::outputSerial().println("prepare fs......\n");
        
        fp = SPIFFS.open(MOTION_FILE, "w+");
        for (i = 0, start_addr = 0; i < MOTION_FILE_SIZE / BUF_SIZE; i++, start_addr += BUF_SIZE)
        {
            write(start_addr, BUF_SIZE, buf, fp);       
        }
        fp.close();
        
        fp = SPIFFS.open(CONFIG_FILE, "w+");
        for (i = 0, start_addr = 0; i < CONFIG_FILE_SIZE / BUF_SIZE; i++, start_addr += BUF_SIZE)
        {
            write(start_addr, BUF_SIZE, buf, fp);      
        }
        fp.close();
        
        fp = SPIFFS.open(SYSCFG_FILE, "w+");
        for (i = 0, start_addr = 0; i < SYSCFG_FILE_SIZE / BUF_SIZE; i++, start_addr += BUF_SIZE)
        {
            write(start_addr, BUF_SIZE, buf, fp);        
        }
        fp.close();
        System::outputSerial().println("fs formated\n");
    }
    fp_motion = SPIFFS.open(MOTION_FILE, "r+");
    fp_config = SPIFFS.open(CONFIG_FILE, "r+");
//    fp_syscfg = SPIFFS.open(SYSCFG_FILE, "a+");
}

void PLEN2::ExternalFs::de_init()
{
    if (fp_motion)
    {
        fp_motion.close();
    }
    if (fp_config)
    {
        fp_config.close();
    }
    if (fp_syscfg)
    {
        fp_syscfg.close();
    }
}

char PLEN2::ExternalFs::read(
    unsigned int start_addr,
    unsigned int size,
    unsigned char data[],File fp)
{
    if (fp)
    {
        fp.seek(start_addr, SeekSet);  
        return fp.read(data, size);
    }
    return -1;
}

char PLEN2::ExternalFs::write(
    unsigned int start_addr,
    unsigned int size,
    const unsigned char data[],File fp)
{
    unsigned int write_size;
    if (fp)
    {
        fp.seek(start_addr, SeekSet);
        write_size = fp.write(data, size);
        fp.flush();
        return write_size == size;
    }
    return -1;
}


unsigned char PLEN2::ExternalFs::readByte(
    unsigned int start_addr,File fp)
{
    unsigned char data;
    if (fp)
    {
        fp.seek(start_addr, SeekSet);  
        data = (unsigned char )fp.read();
        return data;
    }
    return -1;
}

char PLEN2::ExternalFs::writeByte(
    unsigned int start_addr,
    unsigned char data,File fp)
{
    unsigned int write_size;
    if (fp)
    {
        fp.seek(start_addr, SeekSet);
        write_size = fp.write(data);
        fp.flush();
        return write_size == 1;
    }
    return 0;
}

char PLEN2::ExternalFs::readSlot(
	unsigned int  slot,
	unsigned char data[],
	unsigned char read_size,
	File fp
)
{
	#if DEBUG
		volatile Utility::Profiler p(F("ExternalFs::readSlot()"));
	#endif
	if (   (slot >= SLOT_END())
		|| (read_size > SLOT_SIZE() 
		|| !fp)
	) 
	{
		#if DEBUG_LESS
			System::debugSerial().print(F(">>> bad argument! : slot = "));
			System::debugSerial().print(slot);
			System::debugSerial().print(F(", or read_size = "));
			System::debugSerial().print(read_size);
			System::debugSerial().print(F(", or fp = "));
			System::debugSerial().println(fp);
		#endif

		return -1;
	}

	unsigned int data_address = slot * CHUNK_SIZE();

	#if DEBUG
		System::debugSerial().print(F(">>> data_address = "));
		System::debugSerial().print(slot);
		System::debugSerial().print(F(" : "));
		System::debugSerial().println(data_address, HEX);
	#endif
	if (!fp.seek(data_address, SeekSet))
	{
        System::debugSerial().println(F(">>>readSlot Seek Error"));
	}
	read_size = fp.read(data, read_size);
	return read_size;
}


char PLEN2::ExternalFs::writeSlot(
	unsigned int  slot,
	const unsigned char data[],
	unsigned char write_size,
	File fp
)
{
    unsigned char read_data[100];
	#if DEBUG
		volatile Utility::Profiler p(F("ExternalFs::writeSlot()"));
	#endif
	
	if (   (slot >= SLOT_END())
		|| (write_size > SLOT_SIZE()
		|| !fp)
	)
	{
		#if DEBUG_LESS
			System::debugSerial().print(F(">>> bad argument! : slot = "));
			System::debugSerial().print(slot);
			System::debugSerial().print(F(", or write_size = "));
			System::debugSerial().print(write_size);
     		System::debugSerial().print(F(", or fp = "));
			System::debugSerial().println(fp);
		#endif

		return -1;
	}

	unsigned int data_address = slot * CHUNK_SIZE();

	#if DEBUG
		System::debugSerial().print(F(">>> data_address : "));
		System::debugSerial().print(slot);
		System::debugSerial().print(F(" : "));
		System::debugSerial().println(data_address, HEX);
	#endif

	if(!fp.seek(data_address, SeekSet))
	{
        System::debugSerial().println(F(">>>writeSlot: Seek Error"));
	}
	write_size = fp.write(data, write_size);
	fp.flush();
	return 0;
}
