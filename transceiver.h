/***************************************************************************
 *   Copyright (C) 2016 by Terraneo Federico, Fabiano Riccardi and         *
 *      Luigi Rinaldi                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/
#ifndef TRANSCEIVER_H
#define TRANSCEIVER_H

#include <tuple>
#include "SPI.h"

enum class CC2520Register;
enum class CC2520State;

class TransceiverConfiguration
{
public:
	TransceiverConfiguration(int frequency, int txPower=0, bool crc=true)
		: frequency(frequency), txPower(txPower), crc(crc) {}

	int frequency;
	int txPower;
	bool crc;
};

class Transceiver
{
public:
	static Transceiver& instance();

	void turnOn(const TransceiverConfiguration& config);

	void turnOff();

	void reconfigure(const TransceiverConfiguration& config);

	void sendNow(void *pkt, int size);

	bool sendCca(void *pkt, int size);

	void sendAt(void *pkt, int size, long long when);
        
        //timeout fired, timestamp, rssi,effective size
	std::tuple<bool, long long, int, int> recv(void *pkt, int size, long long timeout);

private:
	Transceiver();

	Transceiver(const Transceiver&) = delete;
	Transceiver& operator= (const Transceiver&) = delete;

	void waitXosc();

	void writeReg(CC2520Register reg, unsigned char data);
        
        unsigned char readReg(CC2520Register reg);

	unsigned char txPower(int dBm);

	SPI spi;
	CC2520State state;
        
        void printStatus();
};

#endif //TRANSCEIVER_H
