/***************************************************************************
 *   Copyright (C) 2016 by Fabiano Riccardi                                *
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

#ifndef GPIO_TIMER_TEST_CONST_H
#define GPIO_TIMER_TEST_CONST_H

//Common
const long long timeout = 1000000000000LL;
const long long t10ms = 480000;
const long long t1ms = 48000;
const long long t1s = 48000000;
const long long noticeableValues[]={    0x00000001,
                                        0x00009C40,
                                        0x000000C8,
                                        0x0BEB0001,
                                        0x1000FFFF,
                                        0x1C00FFC0,
                                        0x37359400,
                                        0x5302D241,
                                        0x56E10001,
                                        0x5AA176C1,
                                        0x5CA10000,
                                        0x5FA176C1,
                                        0xFAFFC000,
                                        0x100000000,
                                        0x10A001FFF,
                                        0x1FFFFFFFF
};

//Master
const long long startMaster = 48000000;
const long long offsetBetweenPing = 1920000; //40ms

//Slave
const long long delay = t1ms; //1ms


#endif /* GPIO_TIMER_TEST_CONST_H */

