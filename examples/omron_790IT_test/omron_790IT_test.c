#include "libomron/omron.h"
#include <stdio.h>
#include <stdlib.h>		/* atoi */

int main(int argc, char** argv)
{
	omron_device* test;
	int ret;
	int i;
	int data_count;
	unsigned char str[30];
	int bank = 0;

	if (argc > 1)
		bank = atoi(argv[1]);

	test = omron_create();
	
	ret = omron_get_count(test, OMRON_VID, OMRON_PID);

	if (ret < 0) {
		printf("Cannot scan for devices: %s\n", omron_strerror(ret));
		return 1;
	}
	if (!ret) {
		printf("No omron 790ITs connected!\n");
		return 1;
	}
	printf("Found %d omron 790ITs\n", ret);

	ret = omron_open(test, OMRON_VID, OMRON_PID, 0);
	if (ret < 0) {
		printf("Cannot open omron 790IT: %s\n", omron_strerror(ret));
		return 1;
	}
	printf("Opened omron 790IT\n");

	ret = omron_get_device_version(test, str, sizeof(str));
	if (ret < 0) {
		printf("Cannot get device version: %s\n", omron_strerror(ret));
	} else {
		printf("Device version: %s\n", str);
	}

	ret = omron_get_bp_profile(test, str, sizeof(str));
	if (ret < 0) {
		printf("Cannot get device profile: %s\n", omron_strerror(ret));
	} else {
		printf("Device version: %s\n", str);
	}

	data_count = omron_get_daily_data_count(test, bank);
	printf("AJR data count: %d\n", data_count);
	if (data_count < 0) {
		printf("Cannot get data count: %s\n", omron_strerror(ret));
	}

	for(i = data_count - 1; i >= 0; --i)
	{
		omron_bp_day_info r = omron_get_daily_bp_data(test, bank, i);
		if(!r.present)
		{
			i = i + 1;
			continue;
		}
		printf("%.2d/%.2d/20%.2d %.2d:%.2d:%.2d SYS: %3d DIA: %3d PULSE: %3d\n", r.day, r.month, r.year, r.hour, r.minute, r.second, r.sys, r.dia, r.pulse);
	}


	ret = omron_close(test);
	if (ret < 0) {
		printf("Cannot close omron 790IT: %s\n", omron_strerror(ret));
		return 1;
	}
	return 0;
}
