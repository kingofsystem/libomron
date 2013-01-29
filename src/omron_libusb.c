/*
 * Generic function file for Omron Health User Space Driver - libusb version
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
#include <stdlib.h>

#define OMRON_USB_INTERFACE	0

// FIXME: The following should really be parsed from the HID descriptor instead
//        of being defined as macros..
#define INPUT_REPORT_SIZE 8
#define OUTPUT_REPORT_SIZE 8

omron_device* omron_create_device()
{
	int status;
	omron_device* s = (omron_device*)malloc(sizeof(omron_device));
	s->device._is_open = 0;
	status = libusb_init(&s->device._context);
	if (status < 0) {
		MSG_ERROR("libusb_init returned %d\n", status);
		free(s);
		return NULL;
	}
	return s;
}

int omron_get_count(omron_device* s, int device_vid, int device_pid)
{
	struct libusb_device **devs;
	struct libusb_device *dev;
	size_t i = 0;
	int count = 0;
	int status;

	status = libusb_get_device_list(s->device._context, &devs);
	if (status < 0)
	{
		MSG_ERROR("libusb_get_device_list returned %d\n", status);
		return OMRON_ERR_DEVIO;
	}

	while ((dev = devs[i++]) != NULL)
	{
		struct libusb_device_descriptor desc;
		status = libusb_get_device_descriptor(dev, &desc);
		if (status < 0)
		{
			MSG_WARN("libusb_get_device_descriptor returned %d for device %02x:%02x\n", status, libusb_get_bus_number(dev), libusb_get_device_address(dev));
			break;
		}
		if (desc.idVendor == device_vid && desc.idProduct == device_pid)
		{
			++count;
		}
	}

	libusb_free_device_list(devs, 1);
	return count;
}

int omron_open(omron_device* s, int device_vid, int device_pid, unsigned int device_index)
{
	struct libusb_device **devs;
	struct libusb_device *found = NULL;
	struct libusb_device *dev;
	size_t i = 0;
	int count = 0;
	int status;

	status = libusb_get_device_list(s->device._context, &devs);
	if (status < 0) {
		MSG_ERROR("libusb_get_device_list returned %d\n", status);
		return OMRON_ERR_DEVIO;
	}

	while ((dev = devs[i++]) != NULL)
	{
		struct libusb_device_descriptor desc;
		status = libusb_get_device_descriptor(dev, &desc);
		if (status < 0)
		{
			MSG_ERROR("libusb_get_device_descriptor returned %d for device %02x:%02x\n", status, libusb_get_bus_number(dev), libusb_get_device_address(dev));
			libusb_free_device_list(devs, 1);
			return OMRON_ERR_DEVIO;
		}
		if (desc.idVendor == device_vid && desc.idProduct == device_pid)
		{
			if(count == device_index)
			{
				found = dev;
				break;
			}
			++count;
		}
	}

	if (found)
	{
		dev = found;
		status = libusb_open(found, &s->device._device);
		if (status < 0)
		{
			MSG_ERROR("libusb_open returned %d for device %02x:%02x\n", status, libusb_get_bus_number(dev), libusb_get_device_address(dev));
			libusb_free_device_list(devs, 1);
			return OMRON_ERR_DEVIO;
		}
	}
	else
	{
		MSG_ERROR("Could not find requested device (%d) to open\n", device_index);
		return OMRON_ERR_BADARG;
	}
	MSG_DEVIO("Opened device %d (USB device %02x:%02x)\n", device_index, libusb_get_bus_number(dev), libusb_get_device_address(dev));
	s->input_size = INPUT_REPORT_SIZE;
	s->output_size = OUTPUT_REPORT_SIZE;
	s->device._is_open = 1;

	if(libusb_kernel_driver_active(s->device._device, 0))
	{
		status = libusb_detach_kernel_driver(s->device._device, 0);
		if (status < 0) {
			MSG_WARN("libusb_detach_kernel_driver returned %d for device %02x:%02x\n", status, libusb_get_bus_number(dev), libusb_get_device_address(dev));
		}
	}
	status = libusb_claim_interface(s->device._device, 0);
	if (status < 0) {
		MSG_ERROR("libusb_claim_interface returned %d for device %02x:%02x\n", status, libusb_get_bus_number(dev), libusb_get_device_address(dev));
	}

	return status;
}

int omron_close(omron_device* s)
{
	int status;

	if(!s->device._is_open)
	{
		MSG_ERROR("Device not open\n");
		return OMRON_ERR_NOTOPEN;
	}
	status = libusb_release_interface(s->device._device, 0);
	if (status < 0)
	{
		libusb_device *dev = libusb_get_device(s->device._device);
		MSG_ERROR("libusb_release_interface returned %d for device %02x:%02x\n", status, libusb_get_bus_number(dev), libusb_get_device_address(dev));
		return OMRON_ERR_DEVIO;
	}
	libusb_close(s->device._device);
	s->device._is_open = 0;
	return 0;
}

void omron_delete(omron_device* dev)
{
	free(dev);
}

int omron_set_mode(omron_device* dev, omron_mode mode)
{

	uint8_t feature_report[2] = {(mode & 0xff00) >> 8, (mode & 0x00ff)};
	const uint feature_report_id = 0;
	const uint feature_interface_num = 0;
	const int REQ_HID_SET_REPORT = 0x09;
	const int HID_REPORT_TYPE_FEATURE = 3;
	int num_bytes_transferred;

	MSG_INFO("Setting mode to %04x\n", mode);
	num_bytes_transferred = libusb_control_transfer(dev->device._device, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, REQ_HID_SET_REPORT, (HID_REPORT_TYPE_FEATURE << 8) | feature_report_id, feature_interface_num, feature_report, sizeof(feature_report), 1000);
	if (num_bytes_transferred != sizeof(feature_report)) {
		libusb_device *ludev = libusb_get_device(dev->device._device);
		MSG_ERROR("libusb_control_transfer returned %d for device %02x:%02x\n", num_bytes_transferred, libusb_get_bus_number(ludev), libusb_get_device_address(ludev));
		return OMRON_ERR_DEVIO;
	}
	MSG_DETAIL("Mode set successfully.\n");
	return 0;
}

int omron_read_data(omron_device* dev, uint8_t* report_buf, int report_size, int timeout)
{
	int trans;
	int status;
	int timeout_ok = (timeout < 0);

	if (timeout_ok) {
		timeout = -timeout;
	}
	if (report_size < dev->input_size) {
		MSG_ERROR("Supplied buffer too small (%d < %d)\n", report_size, dev->input_size);
		return OMRON_ERR_BUFSIZE;
	}
	status = libusb_bulk_transfer(dev->device._device, OMRON_IN_ENDPT, report_buf, dev->input_size, &trans, timeout);
	if (status != 0) {
		if (status == LIBUSB_ERROR_TIMEOUT) {
			if (timeout_ok) {
				MSG_DEVIO("(USB operation timed out)\n");
				return 0;
			}
			MSG_ERROR("USB operation timed out.\n");
			return OMRON_ERR_DEVIO;
		}
		MSG_ERROR("libusb_bulk_transfer returned %d\n", status);
		return OMRON_ERR_DEVIO;
	}
	MSG_HEXDUMP(OMRON_DEBUG_DEVIO, "read: ", report_buf, trans);
	if (trans != dev->input_size) {
		MSG_ERROR("Transfer size (%d) did not match expected (%d)\n", trans, dev->input_size);
		return OMRON_ERR_DEVIO;
	}
	return trans;
}

int omron_write_data(omron_device* dev, uint8_t* report_buf, int report_size, int timeout)
{
	int trans;
	int status;
	int timeout_ok = (timeout < 0);

	if (timeout_ok) {
		timeout = -timeout;
	}
	if (report_size > dev->output_size) {
		MSG_ERROR("Supplied buffer too large (%d > %d)\n", report_size, dev->output_size);
		return OMRON_ERR_BUFSIZE;
	}
	status = libusb_bulk_transfer(dev->device._device, OMRON_OUT_ENDPT, report_buf, report_size, &trans, timeout);
	if (status != 0) {
		if (status == LIBUSB_ERROR_TIMEOUT) {
			if (timeout_ok) {
				MSG_DEVIO("(USB operation timed out)\n");
				return 0;
			}
			MSG_ERROR("USB operation timed out.\n");
			return OMRON_ERR_DEVIO;
		}
		MSG_ERROR("libusb_bulk_transfer returned %d\n", status);
		return OMRON_ERR_DEVIO;
	}
	MSG_HEXDUMP(OMRON_DEBUG_DEVIO, "wrote: ", report_buf, trans);
	if (trans != report_size) {
		MSG_ERROR("Transfer size (%d) did not match expected (%d)\n", trans, dev->input_size);
		return OMRON_ERR_DEVIO;
	}
	return trans;
}

