/*
 * Generic function file for Omron Health User Space Driver
 *
 * Copyright (c) 2007-2009 Kyle Machulis/Nonpolynomial Labs <kyle@nonpolynomial.com>
 *
 * More info on Nonpolynomial Labs @ http://www.nonpolynomial.com
 *
 * Sourceforge project @ http://www.github.com/qdot/libomron/
 *
 * This library is covered by the BSD License
 * Read LICENSE_BSD.txt for details.
 */

#include "omron.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

// #define DEBUG

#ifdef DEBUG
#define IF_DEBUG(x)	do { x; } while (0)
#else
#define IF_DEBUG(x)	do { } while (0)
#endif

#define DPRINTF(args...)	IF_DEBUG(printf(args))

static void hexdump(void *data, int n_bytes)
{
	while (n_bytes--) {
		printf(" %02x", *(unsigned char*) data);
		data++;
	}
}

int omron_send_command(omron_device* dev, int size, const unsigned char* buf)
{
	int total_write_size = 0;
	int current_write_size;
	unsigned char output_report[8];

	DPRINTF("omron_send:");
	IF_DEBUG(hexdump(buf, size));
	DPRINTF("\n");

	while(total_write_size < size)
	{
		current_write_size = size - total_write_size;
		if(current_write_size > 7)
			current_write_size = 7;

		output_report[0] = current_write_size;
		memcpy(output_report + 1, buf+total_write_size,
		       current_write_size);

		omron_write_data(dev, output_report);
		total_write_size += current_write_size;
	}
	return 0;
}

int omron_check_success(unsigned char *input_report)
{
	return (input_report[1] == 'O' && input_report[2] == 'K') ? 0 : -1;
}

int omron_send_clear(omron_device* dev)
{
	static const unsigned char zero[23]; /* = all zeroes */
	unsigned char input_report[9];
	int read_result;

	do {
		omron_send_command(dev, 23, zero);
		read_result = omron_read_data(dev, input_report);

	} while (read_result < 2 || omron_check_success(input_report) != 0);

	return 0;
}

static int
xor_checksum(unsigned char *data, int len)
{
	unsigned char checksum = 0;

	while (len--)
		checksum ^= *(data++);

	if (checksum)
		printf("bad checksum 0x%x\n", checksum);

	return checksum;
}

/*
  omron_get_command_return returns:
	#of bytes
  0 : Valid response ("OK..." or "NO").
  1 : Garbled response
  <0 : IO error
*/

int omron_get_command_return(omron_device* dev, int size, unsigned char* data)
{
	int total_read_size = 0;
	int current_read_size = 0;
	int has_checked = 0;
	unsigned char input_report[9];
	int read_result;


	DPRINTF("\n");		/* AJR */
	while(total_read_size < size)
	{
		read_result = omron_read_data(dev, input_report);
		DPRINTF("AJR read result=%d.\n", read_result);
		if (read_result < 0) {
			fprintf(stderr, "omron_get_command_return: read_result_result %d < zero\n", read_result);
			return read_result;
		}

		assert(read_result == 8);
		current_read_size = input_report[0];
		IF_DEBUG(hexdump(input_report, current_read_size+1));
		DPRINTF(" current_read=%d size=%d total_read_size=%d.\n",
		       current_read_size, size, total_read_size);

		assert(current_read_size <= 8);

		if (current_read_size == 8)
			current_read_size = 7; /* FIXME? Bug? */

		assert(current_read_size < 8);
		assert(current_read_size > 0);

		assert(total_read_size >= 0);


		assert(current_read_size <= size - total_read_size);
#if 0
		assert(current_read_size == 7 ||
		       current_read_size == size - total_read_size);
#endif

		memcpy(data + total_read_size, input_report + 1,
		       current_read_size);
		total_read_size += current_read_size;

		if(!has_checked && total_read_size >= 2)
		{
			if (total_read_size == 2 &&
			    data[0] == 'N' && data[1] == 'O')
				return 0; /* "NO" is valid response */

			if (strncmp((const char*) data, "END\r\n",
				    total_read_size) == 0)
				has_checked = (total_read_size >= 5);
			else {
				if (data[0] != 'O' || data[1] != 'K')
					return 1; /* garbled response */
				
				has_checked = 1;
			}
		}
	}

	if (total_read_size >= 3 && data[0] == 'O' && data[1] == 'K')
		return xor_checksum(data+2, total_read_size-2); /* 0 = OK */
	else
		return 0;
}

int omron_check_mode(omron_device* dev, omron_mode mode)
{
	int ret;

#if 0
	/*
	  At least under Linux usbfs, I get inconsistent results from
	  reading data if I uncomment this optimization.  Perhaps
	  the Omron blood pressure meter may forget its current
	  mode at times.
	*/
	if(dev->device_mode == mode)
		return 0;
#endif

	ret = omron_set_mode(dev, mode);
	if(ret == 0)
	{
		dev->device_mode = mode;
		omron_send_clear(dev);
	}
	return ret;
}

static void omron_exchange_cmd(omron_device *dev,
			       omron_mode mode,
			       int cmd_len,
			       const unsigned char *cmd,
			       int response_len,
			       unsigned char *response)
{
	int status;

	// Retry command if the response is garbled, but accept "NO" as
	// a valid response.
	do {
		omron_check_mode(dev, mode);
		omron_send_command(dev, cmd_len, cmd);
		status = omron_get_command_return(dev, response_len, response);
		if (status > 0) {
			printf("garbled (resp_len=%d)\n", response_len);
		}
	} while (status > 0);

	if (status < 0) {
		fprintf(stderr, "omron_exchange_cmd: I/O error, status=%d\n",
			status);
		exit(1);
	}
}

static void
omron_dev_info_command(omron_device* dev,
		       const char *cmd,
		       unsigned char *result,
		       int result_max_len)
{
	unsigned char tmp[result_max_len+3];

	omron_exchange_cmd(dev, DEVICE_INFO_MODE, strlen(cmd),
			   (const unsigned char*) cmd,
			   result_max_len+3, tmp);

	memcpy(result, tmp + 3, result_max_len);
}

//platform independant functions
int omron_get_device_version(omron_device* dev, unsigned char* data)
{
	omron_dev_info_command(dev, "VER00", data, 12);
	data[12] = 0;
	return 0;
}

int omron_get_device_prf(omron_device* dev, unsigned char* data)
{
	omron_dev_info_command(dev, "PRF00", data, 11);
	return 0;
}

int omron_get_device_serial(omron_device* dev, unsigned char* data)
{
	omron_dev_info_command(dev, "SRL00", data, 8);
	return 0;
}

//daily data information
int omron_get_daily_data_count(omron_device* dev, unsigned char bank)
{
	unsigned char data[8];
	unsigned char command[8] =
		{ 'G', 'D', 'C', 0x00, bank, 0x00, 0x00, bank };

	// assert(bank < 2);
	omron_exchange_cmd(dev, DAILY_INFO_MODE, 8, command,
			   sizeof(data), data);
	DPRINTF("Data units found: %d\n", (int)data[6]);
	return (int)data[6];
}

omron_bp_day_info* omron_get_all_daily_bp_data(omron_device* dev, int* count)
{
	omron_check_mode(dev, DAILY_INFO_MODE);
	return 0;
}

omron_bp_day_info omron_get_daily_bp_data(omron_device* dev, int bank, int index)
{
	omron_bp_day_info r;
	unsigned char data[17];
	unsigned char command[8] = {'G', 'M', 'E', 0x00, bank, 0x00,
				    index, index ^ bank};

	omron_exchange_cmd(dev, DAILY_INFO_MODE, 8, command,
			   sizeof(data), data);

	// printf("Daily data:");
	// hexdump(data, sizeof(data));
	// printf("\n");

	r.year = data[3];
	r.month = data[4];
	r.day = data[5];
	r.hour = data[6];
	r.minute = data[7];
	r.second = data[8];
	r.sys = data[11];
	r.dia = data[12];
	r.pulse = data[13];
	return r;
}

//weekly data information
omron_bp_week_info* omron_get_all_weekly_bp_data(omron_device* dev, int* count)
{
	// omron_bp_week_info* r;
	omron_check_mode(dev, DEVICE_INFO_MODE);
	return 0;
}

omron_bp_week_info omron_get_weekly_bp_data(omron_device* dev, int bank, int index, int evening)
{
	omron_bp_week_info r;
	unsigned char data[12];	/* 12? */
	unsigned char command[9] = { 'G',
				     (evening ? 'E' : 'M'),
				     'A',
				     0,
				     bank,
				     index,
				     0,
				     0,
				     index^bank };
				     
	memset(&r, 0 , sizeof(r));
	memset(data, 0, sizeof(data));

	/* FIXME: Use WEEKLY_INFO_MODE? */
	omron_exchange_cmd(dev, WEEKLY_INFO_MODE, 9, command,
			   sizeof(data), data);

	if (data[0] != 'O' || data[1] != 'K')
		r.present = 0;
	else {
		r.present = 1;
		
		// printf("Weekly data:");
		// hexdump(data, sizeof(data));
		// printf("\n");

		r.year = data[4];
		r.month = data[5];
		r.day = data[6];
		r.sys = data[8] + 25;
		r.dia = data[9];
		r.pulse = data[10];
	}
	return r;
}