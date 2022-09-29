/* Very basic M-Bus device that can act as a 'slave'
 *
 * Copyright (C) 2022  Addiva Elektronik AB
 */

#include <err.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>
#include <mbus/mbus.h>

#define MBUS_DEFAULT_SERIAL_DEVICE "/dev/ttyS0"

static char *device = MBUS_DEFAULT_SERIAL_DEVICE;
static int   address;
static int   debug;
static char *arg0;

/* Our fake identity, from std. pp 35 */
unsigned char raw_response[] = {
	0x68, 0x13, 0x13, 0x68, /* header of RSP_UD telegram (L-Field = 13h = 19d) */
	0x08, 0x05, 0x73,	/* C field = 08h (RSP_UD), address 5, CI field = 73h (fixed, LSByte first) */
	0x78, 0x56, 0x34, 0x12, /* identification number = 12345678 */
	0x0a,			/* transmission counter = 0Ah = 10d */
	0x00,			/* status 00h: counters coded BCD, actual values, no errors */
	0xe9, 0x7e,		/* Type&Unit: medium water, unit1 = 1l, unit2 = 1l (same, but historic) */
	0x01, 0x00, 0x00, 0x00, /* counter 1 = 1l (actual value) */
	0x35, 0x01, 0x00, 0x00, /* counter 2 = 135 l (historic value) */
	0x3c, 0x16		/* checksum and stop sign */
};

static void mbus_send_ack(mbus_handle *h)
{
	mbus_frame *f;

	f = mbus_frame_new(MBUS_FRAME_TYPE_ACK);
	if (mbus_send_frame(h, f))
		warnx("Failed packing ACK");
	free(f);
}

static int match_secondary(mbus_frame *f, mbus_frame_data *me)
{
	long long fid, oid;
	int match = 0;
	int i;

	if (f->data_size < 8 || f->data_size > 8) {
		warnx("Invalid scan frame: %s", mbus_error_str());
		return 0;
	}

	fid = mbus_data_bcd_decode_hex(f->data, 4);
	oid = mbus_data_bcd_decode_hex(me->data_var.header.id_bcd, 4);

	for (i = 0; i < 8; i++) {
		int shift = 28 - i * 4;
		char f, o;

		f = (fid >> shift) & 0x0f;
		o = (oid >> shift) & 0x0f;
		if (f == 0xf)
			continue;
		if (f == o)
			match = 1;
	}

	return match;
}

static int usage(int rc)
{
	fprintf(stderr,
		"Usage: %s [-D] [-a ADDR] [-d DEVICE] [-f FILE]\n"
		"\n"
		"Options:\n"
		" -a ADDR    Set primary address, default: 0\n"
		" -D         Enable debug messages\n"
		" -d device  Serial port/pty to use\n"
		" -f file    Test data to reuse, simulate other device\n"
		"\n"
		"Copyright (c) 2022  Addiva Elektronik AB\n", arg0);
	return rc;
}

int main(int argc, char **argv)
{
	mbus_frame ack, request, response;
	mbus_handle *handle;
	unsigned char *buf;
	char *file = NULL;
	size_t len;
	int result;
	int c;


	arg0 = argv[0];
	memset(&request, 0, sizeof(mbus_frame));

	while ((c = getopt(argc, argv, "a:Dd:f:")) != EOF) {
		switch (c) {
		case 'a':
			address = atoi(optarg);
			break;
		case 'D':
			debug = 1;
			break;
		case 'd':
			device = optarg;
			break;
		case 'f':
			file = optarg;
			break;
		default:
			return usage(0);
		}
	}

	handle = mbus_context_serial(device);
	if (!handle) {
		warnx("Failed initializing M-Bus context: %s", mbus_error_str());
		return 1;
	}

	if (debug) {
		mbus_register_send_event(handle, &mbus_dump_send_event);
		mbus_register_recv_event(handle, &mbus_dump_recv_event);
	}

	if (mbus_connect(handle) == -1) {
		errx(1, "Failed opening serial port: %s", mbus_error_str());
		return 1;
	}

	if (file) {
		unsigned char filebuf[1024], binbuf[1024];
		FILE *fp;

		fp = fopen(file, "r");
		if (!fp)
			err(1, "Failed opening %s", file);

		len = fread(filebuf, 1, sizeof(filebuf) - 1, fp);
		filebuf[len] = 0;
		fclose(fp);

		buf = binbuf;
		len = mbus_hex2bin(binbuf, sizeof(binbuf), filebuf, sizeof(filebuf));
	} else {
		buf = raw_response;
		len = sizeof(raw_response);
	}

	result = mbus_parse(&response, buf, len);
	if (result != 0)
		errx(1, "Invalid M-Bus response frame, rc %d: %s", result, mbus_error_str());

	mbus_frame_data *me = mbus_frame_data_new();
	if (!me)
		errx(1, "Failed allocating device data");
	if (mbus_frame_data_parse(&response, me) == -1)
		errx(1, "Failed parsing my identity");

	response.address = address;
	char *secondary = mbus_frame_get_secondary_address(&response);
	if (!secondary)
		warnx("Starting up, primary addr %d, no secondary: %s", address, mbus_error_str());
	else
		warnx("Starting up, primary addr %d, secondary addr %s", address, secondary);

	int selected = 0;
	while (1) {
		int fcb = 0, forus = 0;

		if (mbus_recv_frame(handle, &request) != MBUS_RECV_RESULT_OK)
			continue;

		switch (request.address) {
		case MBUS_ADDRESS_BROADCAST_NOREPLY:
			continue; /* nop */

		case MBUS_ADDRESS_BROADCAST_REPLY:
			break;	/* all should respond */

		case MBUS_ADDRESS_NETWORK_LAYER:
			break;	/* XXX */

		default:
			if (request.address == address) {
				forus = 1;
				break;
			}

			warnx("Not for us (%d), got addr %d control %d", address, request.address, request.control);
			continue;
		}

		if (request.control & MBUS_CONTROL_MASK_FCB) {
			request.control &= ~MBUS_CONTROL_MASK_FCB;
			fcb = 1;
		}

		switch (request.control) {
		case MBUS_CONTROL_MASK_SND_NKE: /* wakeup */
			if (request.address == MBUS_ADDRESS_NETWORK_LAYER)
				selected = 0; /* std. v4.8 ch 7.1 pp 64 */
			mbus_send_ack(handle);
			break;

		case MBUS_CONTROL_MASK_REQ_UD2: /* req data */
			if (!selected && !forus)
				break;

			if (mbus_send_frame(handle, &response))
				warnx("Failed sending response: %s", mbus_error_str());
			break;

		case MBUS_CONTROL_MASK_SND_UD: /* select secondary? */
			switch (request.control_information) {
			case MBUS_CONTROL_INFO_DATA_SEND:
				if (request.data[0] == 0x01 && request.data[1] == 0x7a) {
					warnx("Set new primary address %d", request.data[2]);
					address = request.data[2];
					response.address = address;
					mbus_send_ack(handle);
				}
				break;

			case MBUS_CONTROL_INFO_SELECT_SLAVE:
				if (match_secondary(&request, me)) {
					mbus_send_ack(handle);
					selected = 1;
				} else {
					selected = 0; /* std. v4.8 ch 7.1 pp 64 */
				}
				break;

			default:
				/* unsupported atm */
				break;
			}
			break;

		default:
			warnx("Unsupported request, C %d", request.control);
			continue;
		}
	}

	mbus_disconnect(handle);
	mbus_context_free(handle);

	return 0;
}
