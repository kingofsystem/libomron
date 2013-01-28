/*
 * Generic function file for Omron Health User Space Driver - Windows DDK version (Will not work with WDK)
 *
 * Copyright (c) 2009-2010 Kyle Machulis/Nonpolynomial Labs <kyle@nonpolynomial.com>
 *
 * More info on Nonpolynomial Labs @ http://www.nonpolynomial.com
 *
 * Source code available at http://www.github.com/qdot/libomron/
 *
 * This library is covered by the BSD License
 * Read LICENSE_BSD.txt for details.
 */


#include "libomron/omron.h"
#include "omron_internal.h"

#include <api/setupapi.h>
#include <api/hidsdi.h>
#include <stdlib.h>
#include <stdio.h>

//Application global variables
DWORD								ActualBytesRead;
DWORD								BytesRead;
DWORD								cbBytesRead;
PSP_DEVICE_INTERFACE_DETAIL_DATA	detailData;
DWORD								dwError;
char								FeatureReport[256];
HANDLE								hEventObject;
HANDLE								hDevInfo;
GUID								HidGuid;
OVERLAPPED							HIDOverlapped;
char								InputReport[256];
ULONG								Length;
LPOVERLAPPED						lpOverLap;
BOOL								MyDeviceDetected = FALSE;
TCHAR*								MyDevicePathName;
char								OutputReport[256];
DWORD								ReportType;
ULONG								Required;
TCHAR*								ValueToDisplay;

/* Note: The following ReadFileTimeout and WriteFileTimeout routines require
 *       hFile to have been opened with FILE_FLAG_OVERLAPPED
 */
static BOOL ReadFileTimeout(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, DWORD dwMilliseconds) {
	OVERLAPPED overlapped;
	BOOL result;

	result = ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, NULL, &overlapped);
	if (!result && (GetLastError() != ERROR_IO_PENDING)) {
		return result;
	}
	result = GetOverlappedResultEx(hFile, &overlapped, lpNumberOfBytesRead, dwMilliseconds, FALSE);
	if (!result && (GetLastError() == ERROR_IO_INCOMPLETE)) {
		CancelIo(hFile);
		GetOverlappedResult(hFile, &overlapped, lpNumberOfBytesRead, TRUE);
		SetLastError(ERROR_IO_INCOMPLETE);
	}
	return result;
}

static BOOL WriteFileTimeout(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, DWORD dwMilliseconds) {
	OVERLAPPED overlapped;
	BOOL result;

	result = WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, NULL, &overlapped);
	if (!result && (GetLastError() != ERROR_IO_PENDING)) {
		return result;
	}
	result = GetOverlappedResultEx(hFile, &overlapped, lpNumberOfBytesWritten, dwMilliseconds, FALSE);
	if (!result && (GetLastError() == ERROR_IO_INCOMPLETE)) {
		CancelIo(hFile);
		GetOverlappedResult(hFile, &overlapped, lpNumberOfBytesWritten, TRUE);
		SetLastError(ERROR_IO_INCOMPLETE);
	}
	return result;
}

void GetDeviceCapabilities(HANDLE DeviceHandle, HIDP_CAPS *pCaps)
{
	//Get the Capabilities structure for the device.

	PHIDP_PREPARSED_DATA	PreparsedData;

	/*
	  API function: HidD_GetPreparsedData
	  Returns: a pointer to a buffer containing the information about the device's capabilities.
	  Requires: A handle returned by CreateFile.
	  There's no need to access the buffer directly,
	  but HidP_GetCaps and other API functions require a pointer to the buffer.
	*/

	HidD_GetPreparsedData(DeviceHandle, &PreparsedData);

	/*
	  API function: HidP_GetCaps
	  Learn the device's capabilities.
	  For standard devices such as joysticks, you can find out the specific
	  capabilities of the device.
	  For a custom device, the software will probably know what the device is capable of,
	  and the call only verifies the information.
	  Requires: the pointer to the buffer returned by HidD_GetPreparsedData.
	  Returns: a Capabilities structure containing the information.
	*/

	HidP_GetCaps(PreparsedData, pCaps);

	HidD_FreePreparsedData(PreparsedData);
}

int omron_open_win32(omron_device* dev, int VID, int PID, unsigned int device_index, int get_count)
{
	//Use a series of API calls to find a HID with a specified Vendor IF and Product ID.

	HIDD_ATTRIBUTES						Attributes;
	SP_DEVICE_INTERFACE_DATA			devInfoData;
	HIDP_CAPS							Capabilities;
	BOOL								LastDevice = FALSE;
	int									MemberIndex = 0;
	LONG								Result;
	int									device_count = 0;

	Length = 0;
	detailData = NULL;
	dev->device._dev = NULL;

	/*
	  API function: HidD_GetHidGuid
	  Get the GUID for all system HIDs.
	  Returns: the GUID in HidGuid.
	*/

	HidD_GetHidGuid(&HidGuid);

	/*
	  API function: SetupDiGetClassDevs
	  Returns: a handle to a device information set for all installed devices.
	  Requires: the GUID returned by GetHidGuid.
	*/

	//FIXME: check return value
	hDevInfo=SetupDiGetClassDevs
		(&HidGuid,
		 NULL,
		 NULL,
		 DIGCF_PRESENT|DIGCF_INTERFACEDEVICE);

	devInfoData.cbSize = sizeof(devInfoData);

	//Step through the available devices looking for the one we want.
	//Quit on detecting the desired device or checking all available devices without success.

	MemberIndex = 0;
	LastDevice = FALSE;

	do
	{
		/*
		  API function: SetupDiEnumDeviceInterfaces
		  On return, MyDeviceInterfaceData contains the handle to a
		  SP_DEVICE_INTERFACE_DATA structure for a detected device.
		  Requires:
		  The DeviceInfoSet returned in SetupDiGetClassDevs.
		  The HidGuid returned in GetHidGuid.
		  An index to specify a device.
		*/

		Result=SetupDiEnumDeviceInterfaces
			(hDevInfo,
			 0,
			 &HidGuid,
			 MemberIndex,
			 &devInfoData);

		if (Result != 0)
		{
			//A device has been detected, so get more information about it.

			/*
			  API function: SetupDiGetDeviceInterfaceDetail
			  Returns: an SP_DEVICE_INTERFACE_DETAIL_DATA structure
			  containing information about a device.
			  To retrieve the information, call this function twice.
			  The first time returns the size of the structure in Length.
			  The second time returns a pointer to the data in DeviceInfoSet.
			  Requires:
			  A DeviceInfoSet returned by SetupDiGetClassDevs
			  The SP_DEVICE_INTERFACE_DATA structure returned by SetupDiEnumDeviceInterfaces.

			  The final parameter is an optional pointer to an SP_DEV_INFO_DATA structure.
			  This application doesn't retrieve or use the structure.
			  If retrieving the structure, set
			  MyDeviceInfoData.cbSize = length of MyDeviceInfoData.
			  and pass the structure's address.
			*/

			//Get the Length value.
			//The call will return with a "buffer too small" error which can be ignored.

			Result = SetupDiGetDeviceInterfaceDetail
				(hDevInfo,
				 &devInfoData,
				 NULL,
				 0,
				 &Length,
				 NULL);

			//Allocate memory for the hDevInfo structure, using the returned Length.

 			detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(Length);

			//Set cbSize in the detailData structure.

			detailData -> cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

			//Call the function again, this time passing it the returned buffer size.

			//FIXME: check return value
			Result = SetupDiGetDeviceInterfaceDetail
				(hDevInfo,
				 &devInfoData,
				 detailData,
				 Length,
				 &Required,
				 NULL);

			// Open a handle to the device.
			// To enable retrieving information about a system mouse or keyboard,
			// don't request Read or Write access for this handle.

			/*
			  API function: CreateFile
			  Returns: a handle that enables reading and writing to the device.
			  Requires:
			  The DevicePath in the detailData structure
			  returned by SetupDiGetDeviceInterfaceDetail.
			*/

			
			//FIXME: check return value
			dev->device._dev = CreateFile
				(detailData->DevicePath,
				 GENERIC_READ | GENERIC_WRITE,
				 FILE_SHARE_READ|FILE_SHARE_WRITE,
				 (LPSECURITY_ATTRIBUTES)NULL,
				 OPEN_EXISTING,
				 FILE_FLAG_OVERLAPPED,
				 NULL);

			/*
			  API function: HidD_GetAttributes
			  Requests information from the device.
			  Requires: the handle returned by CreateFile.
			  Returns: a HIDD_ATTRIBUTES structure containing
			  the Vendor ID, Product ID, and Product Version Number.
			  Use this information to decide if the detected device is
			  the one we're looking for.
			*/

			//Set the Size to the number of bytes in the structure.

			Attributes.Size = sizeof(Attributes);

			//FIXME: check return value
			Result = HidD_GetAttributes
				(dev->device._dev,
				 &Attributes);

			//Is it the desired device?

			MyDeviceDetected = FALSE;

			if ((Attributes.VendorID == VID && Attributes.ProductID == PID))
			{
				if(get_count)
				{
					++device_count;
					CloseHandle(dev->device._dev);
				}
				else
				{
					MyDeviceDetected = TRUE;
					MyDevicePathName = detailData->DevicePath;
					//FIXME: check return value?
					GetDeviceCapabilities(dev->device._dev, &Capabilities);
					dev->input_size = Capabilities.InputReportByteLength;
					dev->output_size = Capabilities.OutputReportByteLength;
					break;
				}
			}
			else
			{
				CloseHandle(dev->device._dev);
			}
			free(detailData);
		}  //if (Result != 0)

		else
		{
			LastDevice=TRUE;
		}
		//If we haven't found the device yet, and haven't tried every available device,
		//try the next one.
		MemberIndex = MemberIndex + 1;
	}
	while (!LastDevice);
	SetupDiDestroyDeviceInfoList(hDevInfo);
	if(get_count) return device_count;
	if(MyDeviceDetected) return 0;
	MSG_ERROR("Could not find requested device (%d) to open\n", device_index);
	return OMRON_ERR_DEVIO;
}

OMRON_DECLSPEC int omron_get_count(omron_device* dev, int VID, int PID)
{
	return omron_open_win32(dev, VID, PID, 0, 1);
}

OMRON_DECLSPEC int omron_open(omron_device* dev, int VID, int PID, unsigned int device_index)
{
	return omron_open_win32(dev, VID, PID, device_index, 0);
}

OMRON_DECLSPEC int omron_close(omron_device* dev)
{
	CloseHandle(dev->device._dev);
	return 0;
}

int omron_set_mode(omron_device* dev, omron_mode mode)
{
	char feature_report[3] = {0x0, (mode & 0xff00) >> 8, (mode & 0x00ff)};

	MSG_INFO("Setting mode to %04x\n", mode);
	int ret = HidD_SetFeature(dev->device._dev, feature_report, sizeof(feature_report));
	if(!ret)
	{
		MSG_ERROR("Cannot set feature! %d\n", GetLastError());
		return OMRON_ERR_DEVIO;
	}
	MSG_DETAIL("Mode set successfully.\n");
	return 0;
}

OMRON_DECLSPEC int omron_read_data(omron_device* dev, unsigned char *report_buf, int report_size, int timeout)
{
	BOOL result;
	char read_buf[dev->input_size + 1];
	DWORD trans;
	int timeout_ok = (timeout < 0);

	if (timeout_ok) {
		timeout = -timeout;
	}
	if (report_size < dev->input_size) {
		MSG_ERROR("Supplied buffer too small (%d < %d)\n", report_size, dev->input_size);
		return OMRON_ERR_BUFSIZE;
	}
	result = ReadFileTimeout(dev->device._dev,
			  read_buf,
			  dev->input_size + 1,
			  &trans,
			  timeout);
	if (!result) {
		// Windows uses zero to mean "failed"
		if (GetLastError() == ERROR_IO_INCOMPLETE) {
			if (timeout_ok) {
				MSG_DEVIO("(USB operation timed out)\n");
				return 0;
			}
			MSG_ERROR("USB operation timed out.\n");
			return OMRON_ERR_DEVIO;
		}
		MSG_ERROR("Read failed: %d\n", GetLastError());
		return OMRON_ERR_DEVIO;
	}
	trans--;
	memcpy(report_buf, read_buf + 1, trans);
	MSG_HEXDUMP(OMRON_DEBUG_DEVIO, "read: ", report_buf, trans);
	if (trans != dev->input_size) {
		MSG_ERROR("Transfer size (%d) did not match expected (%d)\n", trans, dev->input_size);
	}
	return trans;
}

OMRON_DECLSPEC int omron_write_data(omron_device* dev, unsigned char *report_buf, int report_size, int timeout)
{
	BOOL result;
	char command[dev->output_size + 1];
	DWORD trans;
	int timeout_ok = (timeout < 0);

	if (timeout_ok) {
		timeout = -timeout;
	}
	if (report_size > dev->output_size) {
		MSG_ERROR("Supplied buffer too large (%d > %d)\n", report_size, dev->output_size);
		return OMRON_ERR_BUFSIZE;
	}
	command[0] = 0x0;
	memcpy(command + 1, report_buf, report_size);
	result = WriteFileTimeout(dev->device._dev,
			   command,
			   report_size,
			   &trans,
			   timeout);
	if (!result) {
		// Windows uses zero to mean "failed"
		if (GetLastError() == ERROR_IO_INCOMPLETE) {
			if (timeout_ok) {
				MSG_DEVIO("(USB operation timed out)\n");
				return 0;
			}
			MSG_ERROR("USB operation timed out.\n");
			return OMRON_ERR_DEVIO;
		}
		MSG_ERROR("Write failed: %d\n", GetLastError());
		return OMRON_ERR_DEVIO;
	}
	MSG_HEXDUMP(OMRON_DEBUG_DEVIO, "wrote: ", report_buf, trans);
	if (trans != dev->output_size) {
		MSG_ERROR("Transfer size (%d) did not match expected (%d)\n", trans, dev->output_size);
		return OMRON_ERR_DEVIO;
	}
	return trans;
}

OMRON_DECLSPEC omron_device* omron_create_device()
{
	omron_device* s = (omron_device*)malloc(sizeof(omron_device));
	s->device._is_open = 0;
	s->device._is_inited = 1;	
	return s;
}

OMRON_DECLSPEC void omron_delete(omron_device* dev)
{
	free(dev);
}
