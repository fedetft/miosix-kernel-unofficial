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
#ifndef CONSTANTS_H
#define CONSTANTS_H

/**
 * Registers
 */
enum class CC2520Register
{
	FRMFILT0         = 0x00,
	FRMFILT1         = 0x01,
	SRCMATCH         = 0x02,
	SRCSHORTEN0      = 0x04,
	SRCSHORTEN1      = 0x05,
	SRCSHORTEN2      = 0x06,
	SRCEXTEN0        = 0x08,
	SRCEXTEN1        = 0x09,
	SRCEXTEN2        = 0x0A,
	FRMCTRL0         = 0x0C,
	FRMCTRL1         = 0x0D,
	RXENABLE0        = 0x0E,
	RXENABLE1        = 0x0F,
	EXCFLAG0         = 0x10,
	EXCFLAG1         = 0x11,
	EXCFLAG2         = 0x12,
	EXCMASKA0        = 0x14,
	EXCMASKA1        = 0x15,
	EXCMASKA2        = 0x16,
	EXCMASKB0        = 0x18,
	EXCMASKB1        = 0x19,
	EXCMASKB2        = 0x1A,
	EXCBINDX0        = 0x1C,
	EXCBINDX1        = 0x1D,
	EXCBINDY0        = 0x1E,
	EXCBINDY1        = 0x1F,
	GPIOCTRL0        = 0x20,
	GPIOCTRL1        = 0x21,
	GPIOCTRL2        = 0x22,
	GPIOCTRL3        = 0x23,
	GPIOCTRL4        = 0x24,
	GPIOCTRL5        = 0x25,
	GPIOPOLARITY     = 0x26,
	GPIOCTRL         = 0x28,
	DPUCON           = 0x2A,
	DPUSTAT          = 0x2C,
	FREQCTRL         = 0x2E,
	FREQTUNE         = 0x2F,
	TXPOWER          = 0x30,
	TXCTRL           = 0x31,
	FSMSTAT0         = 0x32,
	FSMSTAT1         = 0x33,
	FIFOPCTRL        = 0x34,
	FSMCTRL          = 0x35,
	CCACTRL0         = 0x36,
	CCACTRL1         = 0x37,
	RSSI             = 0x38,
	RSSISTAT         = 0x39,
	RXFIRST          = 0x3C,
	RXFIFOCNT        = 0x3E,
	TXFIFOCNT        = 0x3F,
	CHIPID           = 0x40,
	VERSION          = 0x42,
	EXTCLOCK         = 0x44,
	MDMCTRL0         = 0x46,
	MDMCTRL1         = 0x47,
	FREQEST          = 0x48,
	RXCTRL           = 0x4A,
	FSCTRL           = 0x4C,
	FSCAL0           = 0x4E,
	FSCAL1           = 0x4F,
	FSCAL2           = 0x50,
	FSCAL3           = 0x51,
	AGCCTRL0         = 0x52,
	AGCCTRL1         = 0x53,
	AGCCTRL2         = 0x54,
	AGCCTRL3         = 0x55,
	ADCTEST0         = 0x56,
	ADCTEST1         = 0x57,
	ADCTEST2         = 0x58,
	MDMTEST0         = 0x5A,
	MDMTEST1         = 0x5B,
	DACTEST0         = 0x5C,
	DACTEST1         = 0x5D,
	ATEST            = 0x5E,
	DACTEST2         = 0x5F,
	PTEST0           = 0x60,
	PTEST1           = 0x61,
	RESERVED         = 0x62,
	DPUBIST          = 0x7A,
	ACTBIST          = 0x7C,
	RAMBIST          = 0x7E,
};

/**
 * Commands
 */
enum class CC2520Command
{
	SNOP                 = 0x00,
	IBUFLD               = 0x02,
	SIBUFEX              = 0x03, //Command Strobe
	SSAMPLECCA           = 0x04, //Command Strobe
	SRES                 = 0x0f,
	MEMORY_MASK          = 0x0f,
	MEMORY_READ          = 0x10, // MEMRD
	MEMORY_WRITE         = 0x20, // MEMWR
	RXBUF                = 0x30,
	RXBUFCP              = 0x38,
	RXBUFMOV             = 0x32,
	TXBUF                = 0x3A,
	TXBUFCP              = 0x3E,
	RANDOM               = 0x3C,
	SXOSCON              = 0x40,
	STXCAL               = 0x41, //Command Strobe
	SRXON                = 0x42, //Command Strobe
	STXON                = 0x43, //Command Strobe
	STXONCCA             = 0x44, //Command Strobe
	SRFOFF               = 0x45, //Command Strobe
	SXOSCOFF             = 0x46, //Command Strobe
	SFLUSHRX             = 0x47, //Command Strobe
	SFLUSHTX             = 0x48, //Command Strobe
	SACK                 = 0x49, //Command Strobe
	SACKPEND             = 0x4A, //Command Strobe
	SNACK                = 0x4B, //Command Strobe
	SRXMASKBITSET        = 0x4C, //Command Strobe
	SRXMASKBITCLR        = 0x4D, //Command Strobe
	RXMASKAND            = 0x4E,
	RXMASKOR             = 0x4F,
	MEMCP                = 0x50,
	MEMCPR               = 0x52,
	MEMXCP               = 0x54,
	MEMXWR               = 0x56,
	BCLR                 = 0x58,
	BSET                 = 0x59,
	CTR_UCTR             = 0x60,
	CBCMAC               = 0x64,
	UCBCMAC              = 0x66,
	CCM                  = 0x68,
	UCCM                 = 0x6A,
	ECB                  = 0x70,
	ECBO                 = 0x72,
	ECBX                 = 0x74,
	INC                  = 0x78,
	ABORT                = 0x7F,
	REGISTER_READ        = 0x80,
	REGISTER_WRITE       = 0xC0,
};

/**
 * Internal state machine
 */
enum class CC2520State
{
	IDLE,
	DEEPSLEEP,
	RX
};

/**
 * Status bits
 */
enum class CC2520Status
{
	RX_A    = 0x01,  // 1 if RX is active
	TX_A    = 0x02,  // 1 if TX is active
	DPU_L_A = 0x04,  // 1 if low  priority DPU is active
	DPU_H_A = 0x08,  // 1 if high  priority DPU is active
	EXC_B   = 0x10,  // 1 if at least one exception on channel B is raise
	EXC_A   = 0x20,  // 1 if at least one exception on channel A is raise
	RSSI_V  = 0x40,  // 1 if RSSI value is valid
	XOSC    = 0x80   // 1 if XOSC is stable and running
};

/**
 * List of exception: register excflag0
 */
enum class CC2520Exc0
{
	RF_IDLE           = 0x01,
	TX_FRM_DONE       = 0x02,
	TX_ACK_DONE       = 0x04,
	TX_UNDERFLOW      = 0x08,
	TX_OVERFLOW       = 0x10,
	RX_UNDERFLOW      = 0x20,
	RX_OVERFLOW       = 0x40,
	RXENABLE_ZERO     = 0x80
};

/**
 * List of exception: register excflag1
 */
enum class CC2520Exc1
{
	RX_FRM_DONE       = 0x01,
	RX_FRM_ACCEPETED  = 0x02,
	SR_MATCH_DONE     = 0x04,
	SR_MATCH_FOUND    = 0x08,
	FIFOP             = 0x10,
	SFD               = 0x20,
	DPU_DONE_L        = 0x40,
	DPU_DONE_H        = 0x80
};

/**
 * List of exception: register excflag2
 */
enum class CC2520Exc2
{
	MEMADDR_ERROR     = 0x01,
	USAGE_ERROR       = 0x02,
	OPERAND_ERROR     = 0x04,
	SPI_ERROR         = 0x08,
	RF_NO_CLOCK       = 0x10,
	RX_FRM_ABORTED    = 0x20,
	RXBUFMOV_TIMEOUT  = 0x40
};

enum class CC2520TxPower
{
	P_5 = 0xf7,  ///< +5dBm
	P_3 = 0xf2,  ///< +3dBm
	P_2 = 0xab,  ///< +2dBm
	P_1 = 0x13,  ///< +1dBm
	P_0 = 0x32,  ///< +0dBm
	P_m2 = 0x81, ///< -2dBm
	P_m7 = 0x2c, ///< -7dBm
	P_m18 = 0x03 ///< -18dBm
};

#endif //CONSTANTS_H
