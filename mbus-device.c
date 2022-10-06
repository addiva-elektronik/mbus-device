/* Very basic M-Bus device that can act as a 'slave'
 *
 * Copyright (C) 2022  Addiva Elektronik AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <err.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>
#include <mbus/mbus.h>

#define dbg(...) if (debug) warnx(__VA_ARGS__)
#define log(...)            warnx(__VA_ARGS__)

static char *device;
static int   address;
static int   debug;
static char *arg0;

/* Our fake identity, from std. pp 43, variable data structure (mode 1) */
unsigned char raw_response[] = {
	0x68, 0x1f, 0x1f, 0x68, /* header of RSP_UD telegram (length 1fh) */
	0x08, 0x02, 0x72,       /* C field = 08h (RSP_UD), address 2, CI field = 72h */
	0x78, 0x56, 0x34, 0x12, /* identification number = 12345678 */
	0x24, 0x40, 0x01, 0x07,	/* manuf. ID 4024h (PAD in EN 61107), generation 1, water */
	0x55, 0x00, 0x00, 0x00,	/* TC 55h, Status 00h, Signature 0000h */
	0x03, 0x13, 0x15, 0x31, 0x00,       /* Data block 1: unit 0, storage No 0, no tariff, instantaneous volume, 12565 l (24 bit integer */
	0xda, 0x02, 0x3b, 0x13, 0x01,       /* Data block 2: unit 0, storage No 5, no tariff, maximum volume flow, 113 l/h (4 digit BCD) */
	0x8b, 0x60, 0x04, 0x37, 0x18, 0x02, /* Data block 3: unit 1, storage No 0, tariff 2, instantaneous energy, 218,37 kWh (6 digit BCD) */
	0x18, 0x16		/* checksum and stop sign */
};

/*
 * random delay of ACK so multiple devices don't collide on the bus.
 */
static void mbus_send_ack(mbus_handle *h)
{
	mbus_frame *f;

	usleep((rand() % 1000) + 1);

	f = mbus_frame_new(MBUS_FRAME_TYPE_ACK);
	if (mbus_send_frame(h, f))
		warnx("Failed sending ACK");
	free(f);
}

static int match_secondary(mbus_frame *f, mbus_frame_data *me)
{
	unsigned char mask[4];
	unsigned char addr[4];
	long long fid, oid;
	int match = 0;
	int i;

	if (f->data_size < 8 || f->data_size > 8) {
		warnx("Invalid scan frame: %s", mbus_error_str());
		return 0;
	}

	memcpy(mask, f->data, 4);
	memcpy(addr, me->data_var.header.id_bcd, 4);

	fid = mbus_data_bcd_decode_hex(mask, 4);
	oid = mbus_data_bcd_decode_hex(addr, 4);

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

	if (match) {
		dbg("Select by secondary %lld [%02X %02X %02X %02X], me %lld [%02X %02X %02X %02X]",
		    fid, mask[0], mask[1], mask[2], mask[3],
		    oid, addr[0], addr[1], addr[2], addr[3]);
	}

	return match;
}

static int usage(int rc)
{
	fprintf(stderr,
		"Usage: %s [-d] [-a ADDR] [-b RATE] [-f FILE] DEVICE\n"
		"\n"
		"Options:\n"
		" -a ADDR    Set primary address, default: 0\n"
		" -b RATE    Set baudrate: 300, 2400, 9600, default: 2400\n"
		" -d         Enable debug messages\n"
		" -f file    Test data to reuse, simulate another product\n"
		"Arguments:\n"
		" DEVICE     Serial port/pty to use\n"
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
	long rate = 0;
	size_t len;
	int result;
	int c;


	arg0 = argv[0];

	while ((c = getopt(argc, argv, "a:b:df:")) != EOF) {
		switch (c) {
		case 'a':
			address = atoi(optarg);
			break;
		case 'b':
			rate = atol(optarg);
			break;
		case 'd':
			debug = 1;
			break;
		case 'f':
			file = optarg;
			break;
		default:
			return usage(0);
		}
	}

	if (optind >= argc)
		return usage(1);
	device = argv[optind++];

	srand(time(NULL));
	memset(&request, 0, sizeof(mbus_frame));

	handle = mbus_context_serial(device);
	if (!handle) {
		warnx("Failed initializing M-Bus context: %s", mbus_error_str());
		return 1;
	}

	if (debug) {
		mbus_register_send_event(handle, &mbus_dump_send_event);
		mbus_register_recv_event(handle, &mbus_dump_recv_event);
	}

	if (mbus_connect(handle) == -1)
		errx(1, "Failed opening serial port: %s", mbus_error_str());

	if (rate && mbus_serial_set_baudrate(handle, rate) == -1)
		errx(1, "Failed setting baud rate %ld on serial port %s: %s",
		     rate, device, mbus_error_str());

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
	if (!secondary) {
		dbg("Starting up, primary addr %d, no secondary: %s", address, mbus_error_str());
	} else {
		dbg("Starting up, primary addr %d, secondary addr %s", address, secondary);
	}

	int selected = 0;
	while (1) {
		int fcb = 0, forus = 0;

		/*
		 * Workaround.  Currently libmbus is solely focused on
		 * the master role.  As such the mbus_recv_frame() has
		 * safeguards that don't apply to a slave device, which
		 * means we cannot use that function or we lose every
		 * other frame.
		 */
		if (handle->recv(handle, &request) != MBUS_RECV_RESULT_OK)
			continue;
		time(&request.timestamp);

		if (request.start1 == MBUS_FRAME_ACK_START)
			continue;

		switch (request.address) {
		case MBUS_ADDRESS_BROADCAST_NOREPLY:
			continue; /* nop */

		case MBUS_ADDRESS_BROADCAST_REPLY:
			forus = 1;
			break;	/* all should respond */

		case MBUS_ADDRESS_NETWORK_LAYER:
			forus = 1;
			break;

		default:
			if (request.address == address) {
				forus = 1;
				break;
			}

			dbg("Not for us (%d), got addr %d control %d",
			    address, request.address, request.control);
			continue;
		}

		if (request.control & MBUS_CONTROL_MASK_FCB) {
			request.control &= ~MBUS_CONTROL_MASK_FCB;
			fcb = 1;
		}

		switch (request.control) {
		case MBUS_CONTROL_MASK_SND_NKE: /* wakeup */
			dbg("SND_NKE (0x%X)", MBUS_CONTROL_MASK_SND_NKE);
			if (request.address == MBUS_ADDRESS_NETWORK_LAYER)
				selected = 0;   /* std. v4.8 ch 7.1 pp 64 */
			if (forus)              /* std. v4.8 ch 5.4 pp 25 */
				mbus_send_ack(handle);
			break;

		case MBUS_CONTROL_MASK_REQ_UD2: /* req data */
			dbg("REQ_UD2 (0x%X)", MBUS_CONTROL_MASK_REQ_UD2);
			if (!selected && !forus)
				break;

			if (mbus_send_frame(handle, &response))
				warnx("Failed sending response: %s", mbus_error_str());
			break;

		case MBUS_CONTROL_MASK_SND_UD: /* select secondary? */
			switch (request.control_information) {
			case MBUS_CONTROL_INFO_DATA_SEND:
				dbg("SND_UD (0x%X) INFO DATA (0x%X)", MBUS_CONTROL_MASK_SND_UD, MBUS_CONTROL_INFO_DATA_SEND);
				if (request.data[0] == 0x01 && request.data[1] == 0x7a) {
					if (!selected)
						break;

					log("Setting new primary address %d", request.data[2]);
					address = request.data[2];
					response.address = address;
					mbus_send_ack(handle);
				}
				break;

			case MBUS_CONTROL_INFO_SELECT_SLAVE:
				dbg("SND_UD (0x%X) SELECT SLAVE (0x%X)", MBUS_CONTROL_MASK_SND_UD, MBUS_CONTROL_INFO_SELECT_SLAVE);
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
