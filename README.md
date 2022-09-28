Basic M-Bus Device
==================

This project provides a very basic M-Bus end-device to act as a 'slave'
role in a wired M-Bus network.  It builds on [libmbus][1] and provides
enough functionality to:

 - Respond to requests
 - Respond to primary address scanning

Note, secondary address not (yet) supported.


Example
-------

You can use `socat` on a Linux system to emulate a point-to-point serial
connection.  On one side of the connection you run the `mbus-device` and
on the other side you can play around with the various libmbus tools.

In terminal A, run `socat` (as a regular user), take note of the devices
created, `/dev/pts/NUM`, here we have 12 + 13:

    $ socat -d -d pty,rawer pty,rawer
    2022/09/26 08:46:54 socat[528933] N PTY is /dev/pts/12
    2022/09/26 08:46:54 socat[528933] N PTY is /dev/pts/13
    2022/09/26 08:46:54 socat[528933] N starting data transfer loop with FDs [5,5] and [7,7]

In terminal B, start the `mbus-device`, here we use primary address 5:

    $ mbus-device -d /dev/pts/12 -a 5

In terminal C, start a libmbus tool, here we request data:

    $ mbus-serial-request-data /dev/pts/13 5
	<?xml version="1.0" encoding="ISO-8859-1"?>
	<MBusData>
		<SlaveInformation>
			<Id>12345678</Id>
			<Medium>Water</Medium>
			<AccessNumber>10</AccessNumber>
			<Status>00</Status>
		</SlaveInformation>
		<DataRecord id="0">
			<Function>Actual value</Function>
			<Unit>l</Unit>
			<Value>1</Value>
		</DataRecord>
		<DataRecord id="1">
			<Function>Actual value</Function>
			<Unit>reserved but historic</Unit>
			<Value>135</Value>
		</DataRecord>
	</MBusData>


Build & Install
---------------

First install [libmbus][1], using the defaults it gets installed into
`/usr/local`.  Debian/Ubuntu systems support that path out of the box,
but Fedora/RedHat systems may need some special incantations.  The
installed library provides a `libmbus.pc` file, which this project use
to figure out the path to the library and include files.

Issuing

    make

builds `mbus-device` in the current directory.  Install it anywhere on
your system, and make sure to bundle the library .so file if you move
the binary to another system.


Origin & References
-------------------

Made by Addiva Elektronik AB, Sweden.  Available as Open Source under
the MIT license.  Please note, libus has a 3-clause BSD license which
contains the advertising clause.

[1]: https://github.com/rscada/libmbus
