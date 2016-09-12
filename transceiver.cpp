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


#include "transceiver.h"
#include "cc2520_constants.h"
#include <stdexcept>
#include <miosix.h>
#include <stdio.h>
using namespace std;
using namespace miosix;

/**
 * Used by the SPI code to respect tcscks, csckh, tcsnh
 * defined at page 34 of the CC2520 datasheet
 */
static inline void spiDelay()
{
    asm volatile("           nop    \n"
		 "           nop    \n"
		 "           nop    \n");
}

//
// class Transceiver
//

Transceiver& Transceiver::instance()
{
	static Transceiver singleton;
	return singleton;
}

void Transceiver::turnOn(const TransceiverConfiguration& config)
{
	if(config.frequency<2394 || config.frequency>2507)
		throw range_error("config.frequency");

	if(state!=CC2520State::DEEPSLEEP) return;

	//reset device whit RESETn that automatically start crystal osc.
	transceiver::reset::low(); //take low for 0.1ms
	transceiver::vregEn::high();
	usleep(500);
	transceiver::reset::high();
	transceiver::cs::low();
	//wait until SO=1 (clock stable and running)
	waitXosc();
	transceiver::cs::high();

	spiDelay();
	writeReg(CC2520Register::FREQCTRL,config.frequency-2394); //set frequency
	writeReg(CC2520Register::FRMCTRL0,0x40*config.crc); //automatically add FCS
	writeReg(CC2520Register::FRMCTRL1,0x2); //ignore tx underflow exception
	writeReg(CC2520Register::FIFOPCTRL,0x7F); //fifop threshold

	//No 12 symbol timeout after frame reception has ended. (192us)
	writeReg(CC2520Register::FSMCTRL,0x0);

	writeReg(CC2520Register::FRMFILT0,0x00); //disable frame filtering
	writeReg(CC2520Register::FRMFILT1,0x00); //disable frame filtering
	writeReg(CC2520Register::SRCMATCH,0x00); //disable source matching

	//Automatically flush tx buffer when an exception buffer overflow take place
	//writeReg(EXCBINDX0,0x0A);
	//writeReg(EXCBINDX1,0x80 | 0x06 );

	//Setting all exception on channel A
	writeReg(CC2520Register::EXCMASKA0,0xFF);
	writeReg(CC2520Register::EXCMASKA1,0xFF);
	writeReg(CC2520Register::EXCMASKA2,0xFF);

	//Setting SFD, TX_FRM_DONE, RX_FRM_DONE on exception channel B
	writeReg(CC2520Register::EXCMASKB0,
			  static_cast<unsigned char>(CC2520Exc0::TX_FRM_DONE));
	writeReg(CC2520Register::EXCMASKB1,
			  static_cast<unsigned char>(CC2520Exc1::RX_FRM_DONE)
			| static_cast<unsigned char>(CC2520Exc1::SFD));
	writeReg(CC2520Register::EXCMASKB2,0x0);

	//Register that need to update from their default value (as recommended datasheet)
	
	writeReg(CC2520Register::TXPOWER,txPower(config.txPower));
	writeReg(CC2520Register::CCACTRL0,0xF8);
	writeReg(CC2520Register::MDMCTRL0,0x85); //controls modem
	writeReg(CC2520Register::MDMCTRL1,0x14); //controls modem
	writeReg(CC2520Register::RXCTRL,0x3f);
	writeReg(CC2520Register::FSCTRL,0x5a);
	writeReg(CC2520Register::FSCAL1,0x2b);
	writeReg(CC2520Register::AGCCTRL1,0x11);
	writeReg(CC2520Register::ADCTEST0,0x10);
	writeReg(CC2520Register::ADCTEST1,0x0E);
	writeReg(CC2520Register::ADCTEST2,0x03);

	//Setting gpio5 as input on rising edge send command strobe STXON
	writeReg(CC2520Register::GPIOCTRL5,0x80 | 0x08);
	//Setting gpio3 as output exception channel B
	writeReg(CC2520Register::GPIOCTRL3,0x22);

	state=CC2520State::IDLE;
}

void Transceiver::turnOff()
{
	if(state==CC2520State::DEEPSLEEP) return;
	transceiver::reset::low();
	transceiver::vregEn::low();

	state=CC2520State::DEEPSLEEP;
}

void Transceiver::reconfigure(const TransceiverConfiguration& config)
{
	if(config.frequency<2394 || config.frequency>2507)
		throw range_error("config.frequency");

	writeReg(CC2520Register::FREQCTRL,config.frequency-2394);
	writeReg(CC2520Register::FRMCTRL0,0x40*config.crc);
	writeReg(CC2520Register::TXPOWER,txPower(config.txPower));
}

void Transceiver::sendNow(void* pkt, int size)
{

}

bool Transceiver::sendCca(void* pkt, int size)
{

}

void Transceiver::sendAt(void* pkt, int size, long long when)
{

}

Transceiver::Transceiver() : state(CC2520State::DEEPSLEEP) {}

void Transceiver::waitXosc()
{
	//TODO: implement it using interrupts
	while(internalSpi::miso::value()==0) ;
}

void Transceiver::writeReg(CC2520Register reg, unsigned char data)
{
	unsigned char r=static_cast<unsigned char>(reg);
	if(r>0x7e) throw range_error("writeReg out of bounds");
	transceiver::cs::low();
	spiDelay();
	if(r<0x40)
	{
		spi.sendRecv(static_cast<unsigned char>(CC2520Command::REGISTER_WRITE) | r);
	} else {
		spi.sendRecv(static_cast<unsigned char>(CC2520Command::MEMORY_WRITE));
		spi.sendRecv(r);
	}
	spi.sendRecv(data);
	spiDelay();
	transceiver::cs::high();
	spiDelay();
}

unsigned char Transceiver::txPower(int dBm)
{
	CC2520TxPower result;
	if(dBm>=5)       result=CC2520TxPower::P_5;
	else if(dBm>=3)  result=CC2520TxPower::P_3;
	else if(dBm==2)  result=CC2520TxPower::P_2;
	else if(dBm==1)  result=CC2520TxPower::P_1;
	else if(dBm==0)  result=CC2520TxPower::P_0;
	else if(dBm>=-2) result=CC2520TxPower::P_m2;
	else if(dBm>=-7) result=CC2520TxPower::P_m7;
	else             result=CC2520TxPower::P_m18;
	return static_cast<unsigned char>(result);
}

std::tuple<bool, long long, int,int> Transceiver::recv(void *pkt, int size, long long timeout){
    if(state==CC2520State::IDLE){
        printf("Attivo la modalità RX\n");
        unsigned char status;
        transceiver::cs::low();
        spiDelay();
        status=spi.sendRecv(static_cast<unsigned char>(CC2520Command::SRXON)); //SRXON
        transceiver::cs::high();
        spiDelay();
        printf("Status: -%x-\n",status);   
    }else if(state == CC2520State::DEEPSLEEP){
        printf("Errore, il device è spento\n");
    }
    
    printStatus();
    
    unsigned char status;
    printf("receiving...\n"); 
    while(!transceiver::excChB::value());
    printStatus();
    printf("SDF signal received\n");
    writeReg(CC2520Register::EXCFLAG1,!32);
    
    //usleep(100000);//Per simulare la preemption lunga da parte di miosix. Provare a spostarla tra tutte le posizioni comprese tra i 2 while
    status=readReg(CC2520Register::EXCFLAG1);
    printf("%x",status);
    //Questo if serve nel caso avvenga una lunga preemption
    if(!(1&status)){
        printf("IF\n");
        while(!transceiver::excChB::value());
    }    
    printf("Frame done signal received\n");
    printStatus();
    writeReg(CC2520Register::EXCFLAG1,!1);
    
    
    
    
    
    
    
    //Provo a leggere
    transceiver::cs::low();
    spiDelay();
    status = spi.sendRecv(0x30); //Per leggere un byte tra quelli ricevuti
    printf("Provo a ricevere. Stato %u\n",status);
    int len=spi.sendRecv();
    printf("Lunghezza buffer:-%u-\n",len);
    printf("Dato letto:\n");
    char aux[200];
    // -2 perchè c'è il CRC o RSSI
    for(int i=0;i<len;i++){
        aux[i]=spi.sendRecv();
    }
    memDump(aux,len);
    printf("\n");
    transceiver::cs::high();
    spiDelay();
    printStatus();
    
    //Controllo validità CRC
    printf("%x ",aux[len-1]);
    if((aux[len-1]&0x80)){
        printf("CRC is valid\n");
    }else{
        printf("CRC isn't valid\n");
    }
    
    
    //Recupero RSSI e lo converto dal complemento 2 all'unsigned
    int rssi=aux[len-2];
    if(rssi>128){
    	rssi-=256;
    }
    return std::tuple<bool,long long, int,int> (false,0,rssi-76,0); 
}

    
unsigned char Transceiver::readReg(CC2520Register reg){
    transceiver::cs::low();
    spiDelay();
    spi.sendRecv(static_cast<unsigned char>(CC2520Command::REGISTER_READ)|static_cast<unsigned char>(reg)); 
    unsigned int status=spi.sendRecv(0x00);
    transceiver::cs::high();
    spiDelay();
    return status;
}

void Transceiver::printStatus(){   
    printf("Register value -%x-%x-%x-\n",readReg(CC2520Register::EXCFLAG0),readReg(CC2520Register::EXCFLAG1),readReg(CC2520Register::EXCFLAG2));
}

