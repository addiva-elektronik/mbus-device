Basic M-Bus Device
==================

This project provides a very basic M-Bus end-device to act as a 'slave'
role in a wired M-Bus network.  It builds on [libmbus][1] and provides
enough functionality to:

 - Respond to requests
 - Respond to primary address scanning
 - Respond to secondary address scanning
 - Set primary address from secondary address

> **Note:** this slave role device is only used for testing a master.


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

    $ mbus-device -a 5 /dev/pts/12

In terminal C, start a libmbus tool, here we request data:

    $ mbus-serial-request-data /dev/pts/13 5
    mbus_frame_print: Dumping M-Bus frame [type 4, 37 bytes]: 68 1F 1F 68 08 05 72 78 56 34 12 24 40 01 07 55 00 00 00 03 13 15 31 00 DA 02 3B 13 01 8B 60 04 37 18 02 1B 16 
    <?xml version="1.0" encoding="ISO-8859-1"?>
    <MBusData>
        <SlaveInformation>
            <Id>12345678</Id>
            <Manufacturer>PAD</Manufacturer>
            <Version>1</Version>
            <ProductName></ProductName>
            <Medium>Water</Medium>
            <AccessNumber>85</AccessNumber>
            <Status>00</Status>
            <Signature>0000</Signature>
        </SlaveInformation>
        <DataRecord id="0">
            <Function>Instantaneous value</Function>
            <StorageNumber>0</StorageNumber>
            <Unit>Volume (m m^3)</Unit>
            <Value>12565</Value>
            <Timestamp>2022-09-29T14:40:22Z</Timestamp>
        </DataRecord>
        <DataRecord id="1">
            <Function>Maximum value</Function>
            <StorageNumber>5</StorageNumber>
            <Tariff>0</Tariff>
            <Device>0</Device>
            <Unit>Volume flow (m m^3/h)</Unit>
            <Value>113</Value>
            <Timestamp>2022-09-29T14:40:22Z</Timestamp>
        </DataRecord>
        <DataRecord id="2">
            <Function>Instantaneous value</Function>
            <StorageNumber>0</StorageNumber>
            <Tariff>2</Tariff>
            <Device>1</Device>
            <Unit>Energy (10 Wh)</Unit>
            <Value>21837</Value>
            <Timestamp>2022-09-29T14:40:22Z</Timestamp>
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
the MIT license.  Please note, libmbus has a 3-clause BSD license which
contains the advertising clause.

[1]: https://github.com/rscada/libmbus
