 
/***************************************************************************
 *   Copyright (C)  2016 by Fabiano Riccardi                               *
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

/**
 * Constant for 802.15.4, updated to 2015 revision.
 */

#ifndef CONSTANTS_802_15_4
#define CONSTANTS_802_15_4

#define _CONTROL_FIELD_MODE_SHIFT                  0
#define _CONTROL_FIELD_MODE_MASK                   (0x07 << _CONTROL_FIELD_MODE_SHIFT)
#define _CONTROL_FIELD_MODE_BEACON                 0x00
#define _CONTROL_FIELD_MODE_DATA                   0x01
#define _CONTROL_FIELD_MODE_ACK                    0x02
#define _CONTROL_FIELD_MODE_MAC_CMD                0x03
#define _CONTROL_FIELD_MODE_RES                    0x04
#define _CONTROL_FIELD_MODE_MULTIPURPOSE           0x05
#define _CONTROL_FIELD_MODE_FRAGMENT               0x06
#define _CONTROL_FIELD_MODE_EXTENDED               0x07
#define CONTROL_FIELD_MODE_BEACON                  (_CONTROL_FIELD_MODE_BEACON << _CONTROL_FIELD_MODE_SHIFT)
#define CONTROL_FIELD_MODE_DATA                    (_CONTROL_FIELD_MODE_DATA << _CONTROL_FIELD_MODE_SHIFT)
#define CONTROL_FIELD_MODE_ACK                     (_CONTROL_FIELD_MODE_ACK << _CONTROL_FIELD_MODE_SHIFT)
#define CONTROL_FIELD_MODE_MAC_CMD                 (_CONTROL_FIELD_MODE_MAC_CMD << _CONTROL_FIELD_MODE_SHIFT)
#define CONTROL_FIELD_MODE_RES                     (_CONTROL_FIELD_MODE_RES << _CONTROL_FIELD_MODE_SHIFT)
#define CONTROL_FIELD_MODE_MULTIPURPOSE            (_CONTROL_FIELD_MODE_MULTIPURPOSE << _CONTROL_FIELD_MODE_SHIFT)
#define CONTROL_FIELD_MODE_FRAGMENT                (_CONTROL_FIELD_MODE_FRAGMENT << _CONTROL_FIELD_MODE_SHIFT)
#define CONTROL_FIELD_MODE_EXTENDED                (_CONTROL_FIELD_MODE_EXTENDED << _CONTROL_FIELD_MODE_SHIFT)

#define _CONTROL_FIELD_SECURITY_EN_SHIFT           3
#define _CONTROL_FIELD_SECURITY_EN_MASK            (0x1 << _CONTROL_FIELD_SECURITY_EN_SHIFT)
#define CONTROL_FIELD_SECURITY_EN                  (0x1 << _CONTROL_FIELD_SECURITY_EN_SHIFT)

#define _CONTROL_FIELD_FRAME_PENDING_SHIFT         4                 
#define _CONTROL_FIELD_FRAME_PENDING_MASK          (0x1 << _CONTROL_FIELD_FRAME_PENDING_SHIFT)
#define CONTROL_FIELD_FRAME_PENDING                (0x1 << _CONTROL_FIELD_FRAME_PENDING_SHIFT)

#define _CONTROL_FIELD_FRAME_ACK_REQ_SHIFT         5                 
#define _CONTROL_FIELD_FRAME_ACK_REQ_MASK          (0x1 << _CONTROL_FIELD_FRAME_ACK_REQ_SHIFT)
#define CONTROL_FIELD_ACK_REQ                      (0x1 << _CONTROL_FIELD_FRAME_ACK_REQ_SHIFT)

#define _CONTROL_FIELD_PANID_COMPRESSION_SHIFT     6                 
#define _CONTROL_FIELD_PANID_COMPRESSION_MASK      (0x0001 << _CONTROL_FIELD_PANID_COMPRESSION_SHIFT)
#define CONTROL_FIELD_PANID_COMPRESSION            (0x0001 << _CONTROL_FIELD_PANID_COMPRESSION_SHIFT)

#define _CONTROL_FIELD_SEQNO_SUPPRESSION_SHIFT     8                 
#define _CONTROL_FIELD_SEQNO_SUPPRESSION_MASK      (0x1 << _CONTROL_FIELD_SEQNO_SUPPRESSION_SHIFT)
#define CONTROL_FIELD_SEQNO_SUPPRESSION            (0x1 << _CONTROL_FIELD_SEQNO_SUPPRESSION_SHIFT)

#define _CONTROL_FIELD_INFORMATION_ELEMENTS_SHIFT  9                 
#define _CONTROL_FIELD_INFORMATION_ELEMENT_MASK    (0x1 << _CONTROL_FIELD_INFORMATION_ELEMENTS_SHIFT)
#define CONTROL_FIELD_INFORMATION_ELEMENT          (0x1 << _CONTROL_FIELD_INFORMATION_ELEMENTS_SHIFT)

#define _CONTROL_FIELD_DEST_ADDR_MODE_SHIFT        10                 
#define _CONTROL_FIELD_DEST_ADDR_MODE_MASK         (0x3 << _CONTROL_FIELD_DEST_ADDR_MODE_SHIFT)
#define _CONTROL_FIELD_DEST_ADDR_MODE_NOT_PRESENT  0x0 
#define _CONTROL_FIELD_DEST_ADDR_MODE_RES          0x1             
#define _CONTROL_FIELD_DEST_ADDR_MODE_16BIT        0x2             
#define _CONTROL_FIELD_DEST_ADDR_MODE_64BIT        0x3 
#define CONTROL_FIELD_DEST_ADDR_MODE_NOT_PRESENT   (_CONTROL_FIELD_DEST_ADDR_MODE_NOT_PRESENT << _CONTROL_FIELD_DEST_ADDR_MODE_MASK)
#define CONTROL_FIELD_DEST_ADDR_MODE_RES           (_CONTROL_FIELD_DEST_ADDR_MODE_RES << _CONTROL_FIELD_DEST_ADDR_MODE_MASK)         
#define CONTROL_FIELD_DEST_ADDR_MODE_16BIT         (_CONTROL_FIELD_DEST_ADDR_MODE_16BIT << _CONTROL_FIELD_DEST_ADDR_MODE_MASK)           
#define CONTROL_FIELD_DEST_ADDR_MODE_64BIT         (_CONTROL_FIELD_DEST_ADDR_MODE_64BIT << _CONTROL_FIELD_DEST_ADDR_MODE_MASK)

#define _CONTROL_FIELD_FRAME_VERSION_SHIFT         12                 
#define _CONTROL_FIELD_FRAME_VERSION_MASK          (0x3 << _CONTROL_FIELD_FRAME_VERSION_SHIFT)
#define _CONTROL_FIELD_FRAME_VERSION_2003          0x0 
#define _CONTROL_FIELD_FRAME_VERSION_2006          0x1             
#define _CONTROL_FIELD_FRAME_VERSION_LAST          0x2             
#define _CONTROL_FIELD_FRAME_VERSION_RES           0x3 
#define CONTROL_FIELD_FRAME_VERSION_2003           (_CONTROL_FIELD_FRAME_VERSION_2003 << _CONTROL_FIELD_FRAME_VERSION_MASK)
#define CONTROL_FIELD_FRAME_VERSION_2006           (_CONTROL_FIELD_FRAME_VERSION_2006 << _CONTROL_FIELD_FRAME_VERSION_MASK)         
#define CONTROL_FIELD_FRAME_VERSION_LAST           (_CONTROL_FIELD_FRAME_VERSION_LAST << _CONTROL_FIELD_FRAME_VERSION_MASK)           
#define CONTROL_FIELD_FRAME_VERSION_RES            (_CONTROL_FIELD_FRAME_VERSION_RES << _CONTROL_FIELD_FRAME_VERSION_MASK)

#define _CONTROL_FIELD_SOURCE_ADDR_MODE_SHIFT        14                 
#define _CONTROL_FIELD_SOURCE_ADDR_MODE_MASK         (0x3 << _CONTROL_FIELD_SOURCE_ADDR_MODE_SHIFT)
#define _CONTROL_FIELD_SOURCE_ADDR_MODE_NOT_PRESENT  0x0 
#define _CONTROL_FIELD_SOURCE_ADDR_MODE_RES          0x1             
#define _CONTROL_FIELD_SOURCE_ADDR_MODE_16BIT        0x2             
#define _CONTROL_FIELD_SOURCE_ADDR_MODE_64BIT        0x3 
#define CONTROL_FIELD_SOURCE_ADDR_MODE_NOT_PRESENT   (_CONTROL_FIELD_SOURCE_ADDR_MODE_NOT_PRESENT << _CONTROL_FIELD_SOURCE_ADDR_MODE_MASK)
#define CONTROL_FIELD_SOURCE_ADDR_MODE_RES           (_CONTROL_FIELD_SOURCE_ADDR_MODE_RES << _CONTROL_FIELD_SOURCE_ADDR_MODE_MASK)         
#define CONTROL_FIELD_SOURCE_ADDR_MODE_16BIT         (_CONTROL_FIELD_SOURCE_ADDR_MODE_16BIT << _CONTROL_FIELD_SOURCE_ADDR_MODE_MASK)           
#define CONTROL_FIELD_SOURCE_ADDR_MODE_64BIT         (_CONTROL_FIELD_SOURCE_ADDR_MODE_64BIT << _CONTROL_FIELD_SOURCE_ADDR_MODE_MASK)

#endif //CONSTANTS_802_15_4