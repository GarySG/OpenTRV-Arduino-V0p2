/*
The OpenTRV project licenses this file to you
under the Apache Licence, Version 2.0 (the "Licence");
you may not use this file except in compliance
with the Licence. You may obtain a copy of the Licence at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the Licence is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied. See the Licence for the
specific language governing permissions and limitations
under the Licence.

Author(s) / Copyright (s): Damon Hart-Davis 2013
*/

/*
 RFM22/RFM23 wireless tranceiver module support.
 */

#ifndef RFM22_H
#define RFM22_H

#include "V0p2_Main.h"

// Minimal set-up of I/O (etc) after system power-up.
// Performs a software reset and leaves the radio deselected and in a low-power and safe state.
void RFM22PowerOnInit();

// Enter standby mode (consume least possible power but retain register contents).
// FIFO state and pending interrupts are cleared.
// Typical consumption in standby 450nA (cf 15nA when shut down, 8.5mA TUNE, 18--80mA RX/TX).
void RFM22ModeStandbyAndClearState();

// Returns true iff RFM22 (or RFM23) appears to be correctly connected.
bool RFM22CheckConnected();

// Configure the radio from a list of register/value pairs in readonly PROGMEM/Flash, terminating with an 0xff register value.
// NOTE: argument is not a pointer into SRAM, it is into PROGMEM!
void RFM22RegisterBlockSetup(const uint8_t registerValues[][2]);

// Transmit contents of on-chip TX FIFO: caller should revert to low-power standby mode (etc) if required.
// Returns true if packet apparently sent correctly/fully.
// Does not clear TX FIFO (so possible to re-send immediately).
// Note: Reliability possibly helped by early move to 'tune' mode to work other than with default (4MHz) lowish PICAXE clock speeds.
bool RFM22TXFIFO();

// Clears the RFM22 TX FIFO and queues up ready to send via the TXFIFO the 0xff-terminated bytes starting at bptr.
// This routine does not change the command area.
void RFM22QueueCmdToFF(const uint8_t *bptr);

#endif
