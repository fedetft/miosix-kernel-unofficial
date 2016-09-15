/***************************************************************************
 *   Copyright (C) 2012, 2013 by Terraneo Federico                         *
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

#ifndef PROTOCOL_CONSTANTS_H
#define	PROTOCOL_CONSTANTS_H

//Channel bandwidth 250 Kbps
const unsigned long long channelbps=250000;


#ifndef USE_VHT
const unsigned long long hz=32768;
#else //USE_VHT
const unsigned long long hz=48000000;
#endif //USE_VHT

//Sync period
const unsigned long long nominalPeriod=static_cast<unsigned long long>(10*hz+0.5f); //@@ Filled in by mkpackage.pl

//Sync window (fixed window), or maximum sync window (dynamic window)
const unsigned long long w=static_cast<unsigned long long>(0.006f*hz+0.5f);
//
//Minimum sync window (dynamic window only)
#ifndef USE_VHT
const unsigned long long minw=static_cast<unsigned long long>(0.001f*hz+0.5f);
#else //USE_VHT
const unsigned long long minw=static_cast<unsigned long long>(0.00003f*hz+0.5f);
#endif //USE_VHT
//
//Receiver is ready 192us after RX are enabled but to eliminate jitterSoftware we set it to 250us
const unsigned long long rxTurnaroundTime=static_cast<unsigned long long>(0.000250*hz+0.5f);
//
#ifndef USE_VHT
//Additional delay to absorb jitter (must be greater than pllBoot+radioBoot)
const unsigned long long jitterAbsorption=static_cast<unsigned long long>(0.005f*hz+0.5f); 
#else //USE_VHT
//Additional delay to absorb jitter (must be greater than pllBoot+radioBoot)
//Also needs to account for vht resynchronization time
const unsigned long long jitterAbsorption=static_cast<unsigned long long>(0.0015f*hz+0.5f); 
#endif //USE_VHT

//Time to transfer a 4 preamble + 1 sfd byte on an 250Kbps channel
const unsigned long long preambleFrameTime=static_cast<unsigned long long>((5*8*hz)/channelbps+0.5f); 
// 
//Time to transfer piggybacking
const unsigned long long piggybackingTime=static_cast<unsigned long long>((0*8*hz)/channelbps+0.5f);
////Time to transfer full packet
const unsigned long long frameTime=static_cast<unsigned long long>((8*8*hz)/channelbps+0.5f);

//
////Time to wait before forwarding the packet
const unsigned long long delayRebroadcastTime=static_cast<unsigned long long>(0.0005f*hz+0.5f); 

#endif //PROTOCOL_CONSTANTS_H
