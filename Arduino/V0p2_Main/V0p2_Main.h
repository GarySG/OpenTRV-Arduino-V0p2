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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
*/

/*
  V0p2 (V0.2) core/main header file for this project:
  all other project header files should #include this first
  (or at least immediately after std/AVR headers) for consistency,
  and project non-header files should include this via their own header files (or directly).
  */

#ifndef V0p2_MAIN_H
#define V0p2_MAIN_H


// GLOBAL flags that alter system build and behaviour.
//#define DEBUG // If defined, do extra checks and serial logging.  Will take more code space and power.
//#define EST_CPU_DUTYCYCLE // If defined, estimate CPU duty cycle and thus base power consumption.

#ifndef BAUD
#define BAUD 4800 // Ensure that OpenTRV 'standard' UART speed is set unless explicitly overridden.
#endif

#include "V0p2_Generic_Config.h" // Config switches and module dependencies.
#include <OTV0p2_Board_IO_Config.h> // I/O pin allocation and setup: include ahead of I/O module headers.

#include <Arduino.h>
#include <OTV0p2Base.h>
#include <OTRadValve.h>
#include <OTRadioLink.h>
#include <OTRFM23BLink.h>
#include <OTSIM900Link.h>
#include <OTRN2483Link.h>

// Indicate that the system is broken in an obvious way (distress flashing of the main UI LED).
// DOES NOT RETURN.
// Tries to turn off most stuff safely that will benefit from doing so, but nothing too complex.
// Tries not to use lots of energy so as to keep the distress beacon running for a while.
void panic();
// Panic with fixed message.
void panic(const __FlashStringHelper *s);

// Call this to do an I/O poll if needed; returns true if something useful happened.
// This call should typically take << 1ms at 1MHz CPU.
// Does not change CPU clock speeds, mess with interrupts (other than possible brief blocking), or sleep.
// Should also do nothing that interacts with Serial.
// Limits actual poll rate to something like once every 8ms, unless force is true.
//   * force if true then force full poll on every call (ie do not internally rate-limit)
// Not thread-safe, eg not to be called from within an ISR.
// NOTE: implementation may not be in power-management module.
bool pollIO(bool force = false);

#ifndef DEBUG
#define DEBUG_SERIAL_PRINT(s) // Do nothing.
#define DEBUG_SERIAL_PRINTFMT(s, format) // Do nothing.
#define DEBUG_SERIAL_PRINT_FLASHSTRING(fs) // Do nothing.
#define DEBUG_SERIAL_PRINTLN_FLASHSTRING(fs) // Do nothing.
#define DEBUG_SERIAL_PRINTLN() // Do nothing.
#define DEBUG_SERIAL_TIMESTAMP() // Do nothing.
#else
// Send simple string or numeric to serial port and wait for it to have been sent.
// Make sure that Serial.begin() has been invoked, etc.
#define DEBUG_SERIAL_PRINT(s) { OTV0P2BASE::serialPrintAndFlush(s); }
#define DEBUG_SERIAL_PRINTFMT(s, fmt) { OTV0P2BASE::serialPrintAndFlush((s), (fmt)); }
#define DEBUG_SERIAL_PRINT_FLASHSTRING(fs) { OTV0P2BASE::serialPrintAndFlush(F(fs)); }
#define DEBUG_SERIAL_PRINTLN_FLASHSTRING(fs) { OTV0P2BASE::serialPrintlnAndFlush(F(fs)); }
#define DEBUG_SERIAL_PRINTLN() { OTV0P2BASE::serialPrintlnAndFlush(); }
// Print timestamp with no newline in format: MinutesSinceMidnight:Seconds:SubCycleTime
extern void _debug_serial_timestamp();
#define DEBUG_SERIAL_TIMESTAMP() _debug_serial_timestamp()
#endif // DEBUG


////// MESSAGING

#ifdef ENABLE_RADIO_PRIMARY_MODULE
extern OTRadioLink::OTRadioLink &PrimaryRadio;
#endif // ENABLE_RADIO_PRIMARY_MODULE

#ifdef ENABLE_RADIO_SECONDARY_MODULE
extern OTRadioLink::OTRadioLink &SecondaryRadio;
#endif // RADIO_SECONDARY_MODULE_TYPE

#ifdef ENABLE_RADIO_SIM900
//For EEPROM:
//- Set the first field of SIM900LinkConfig to true.
//- The configs are stored as \0 terminated strings starting at 0x300.
//- You can program the eeprom using ./OTRadioLink/dev/utils/sim900eepromWrite.ino

extern const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config;
#endif // ENABLE_RADIO_SIM900

#define RFM22_PREAMBLE_BYTE 0xaa // Preamble byte for RFM22/23 reception.
#define RFM22_PREAMBLE_MIN_BYTES 4 // Minimum number of preamble bytes for reception.
#define RFM22_PREAMBLE_BYTES 5 // Recommended number of preamble bytes for reliable reception.
#define RFM22_SYNC_BYTE 0xcc // Sync-word trailing byte (with FHT8V primarily).
#define RFM22_SYNC_MIN_BYTES 3 // Minimum number of sync bytes.

// Send the underlying stats binary/text 'whitened' message.
// This must be terminated with an 0xff (which is not sent),
// and no longer than STATS_MSG_MAX_LEN bytes long in total (excluding the terminating 0xff).
// This must not contain any 0xff and should not contain long runs of 0x00 bytes.
// The message to be sent must be written at an offset of STATS_MSG_START_OFFSET from the start of the buffer.
// This routine will alter the content of the buffer for transmission,
// and the buffer should not be re-used as is.
//   * doubleTX  double TX to increase chance of successful reception
//   * RFM23BfriendlyPremable  if true then add an extra preamble
//     to allow RFM23B-based receiver to RX this
// This will use whichever transmission medium/carrier/etc is available.
#define STATS_MSG_START_OFFSET (RFM22_PREAMBLE_BYTES + RFM22_SYNC_MIN_BYTES)
#define STATS_MSG_MAX_LEN (64 - STATS_MSG_START_OFFSET)
#if defined(ENABLE_RFM23B_FS20_RAW_PREAMBLE)
void RFM22RawStatsTXFFTerminated(uint8_t *buf, bool doubleTX, bool RFM23BFramed = true);
#endif
#if defined(ENABLE_RFM23B_FS20_RAW_PREAMBLE)
// Adds the STATS_MSG_START_OFFSET preamble to enable reception by a remote RFM22B/RFM23B.
// Returns the first free byte after the preamble.
inline uint8_t *RFM22RXPreambleAdd(uint8_t *buf)
  {
  // Start with RFM23-friendly preamble which ends with with the aacccccc sync word.
  memset(buf, RFM22_PREAMBLE_BYTE, RFM22_PREAMBLE_BYTES);
  buf += RFM22_PREAMBLE_BYTES;
  // Send the sync bytes.
  memset(buf, RFM22_SYNC_BYTE, RFM22_SYNC_MIN_BYTES);
  buf += RFM22_SYNC_MIN_BYTES;
  // Return the adjusted pointer.
  return(buf);
  }
#endif

// Returns true if an unencrypted trailing static payload and similar (eg bare stats transmission) is permitted.
// True if the TX_ENABLE value is no higher than stTXmostUnsec.
// Some filtering may be required even if this is true.
#if defined(ENABLE_STATS_TX)
#if !defined(ENABLE_ALWAYS_TX_ALL_STATS)
// TODO: allow cacheing in RAM for speed.
inline bool enableTrailingStatsPayload() { return(OTV0P2BASE::getStatsTXLevel() <= OTV0P2BASE::stTXmostUnsec); }
#else
#define enableTrailingStatsPayload() (true) // Always allow at least some stats to be TXed.
#endif // !defined(ENABLE_ALWAYS_TX_ALL_STATS)
#else
#define enableTrailingStatsPayload() (false)
#endif

#if defined(ENABLE_RADIO_RX)
// Incrementally poll and process I/O and queued messages, including from the radio link.
// Returns true if some work was done.
// This may mean printing them to Serial (which the passed Print object usually is),
// or adjusting system parameters,
// or relaying them elsewhere, for example.
// This will write any output to the supplied Print object,
// typically the Serial output (which must be running if so).
// This will attempt to process messages in such a way
// as to avoid internal overflows or other resource exhaustion,
// which may mean deferring work at certain times
// such as the end of minor cycle.
// The Print object pointer must not be NULL.
bool handleQueuedMessages(Print *p, bool wakeSerialIfNeeded, OTRadioLink::OTRadioLink *rl);
#else
#define handleQueuedMessages(p, wakeSerialIfNeeded, rl) (false)
#endif


/////// CONTROL (EARLY, NOT DEPENDENT ON OTHER SENSORS)

// Radiator valve mode (FROST, WARM, BAKE).
extern OTRadValve::ValveMode valveMode;

// FIXME Moved up from line 464 to fix compilation errors (OccupancyTracker needed on line 261)
// IF DEFINED: support for general timed and multi-input occupancy detection / use.
#ifdef ENABLE_OCCUPANCY_SUPPORT
typedef OTV0P2BASE::PseudoSensorOccupancyTracker OccupancyTracker;
#else
// Placeholder class with dummy static status methods to reduce code complexity.
typedef OTV0P2BASE::DummySensorOccupancyTracker OccupancyTracker;
#endif
// Singleton implementation for entire node.
extern OccupancyTracker Occupancy;


////// SENSORS

// Sensor for supply (eg battery) voltage in millivolts.
// Singleton implementation/instance.
extern OTV0P2BASE::SupplyVoltageCentiVolts Supply_cV;

#ifdef ENABLE_TEMP_POT_IF_PRESENT
#if (V0p2_REV >= 2) && defined(TEMP_POT_AIN) // Only supported in REV2/3/4/7.
#define TEMP_POT_AVAILABLE
// Sensor for temperature potentiometer/dial UI control.
typedef OTV0P2BASE::SensorTemperaturePot
    <
    decltype(Occupancy), &Occupancy,
#if (V0p2_REV == 7)
    48, 296, // Correct for DORM1/TRV1 with embedded REV7.
    false // REV7 does not drive pot from IO_POWER_UP.
#elif defined(TEMP_POT_REVERSE)
    1023, 0
#endif
    > TempPot_t;
extern TempPot_t TempPot;
#endif
#endif // ENABLE_TEMP_POT_IF_PRESENT

// Sense ambient lighting level.
#ifdef ENABLE_AMBLIGHT_SENSOR
// Sensor for ambient light level; 0 is dark, 255 is bright.
typedef OTV0P2BASE::SensorAmbientLight AmbientLight;
#else // !defined(ENABLE_AMBLIGHT_SENSOR)
typedef OTV0P2BASE::DummySensorAmbientLight AmbientLight; // Dummy stand-in.
#endif // ENABLE_AMBLIGHT_SENSOR
// Singleton implementation/instance.
extern AmbientLight AmbLight;

// Create very light-weight standard-speed OneWire(TM) support if a pin has been allocated to it.
// Meant to be similar to use to OneWire library V2.2.
// Supports search but not necessarily CRC.
// Designed to work with 1MHz/1MIPS CPU clock.
#if defined(ENABLE_MINIMAL_ONEWIRE_SUPPORT)
#define SUPPORTS_MINIMAL_ONEWIRE
extern OTV0P2BASE::MinimalOneWire<> MinOW_DEFAULT_OWDQ;
#endif

// Cannot have internal and external use of same DS18B20 at same time...
#if defined(ENABLE_EXTERNAL_TEMP_SENSOR_DS18B20) && !defined(ENABLE_PRIMARY_TEMP_SENSOR_DS18B20) && defined(ENABLE_MINIMAL_ONEWIRE_SUPPORT)
#define SENSOR_EXTERNAL_DS18B20_ENABLE_0 // Enable sensor zero.
extern OTV0P2BASE::TemperatureC16_DS18B20 extDS18B20_0;
#endif

// Ambient/room temperature sensor, usually on main board.
#if defined(ENABLE_PRIMARY_TEMP_SENSOR_SHT21)
extern OTV0P2BASE::RoomTemperatureC16_SHT21 TemperatureC16; // SHT21 impl.
#elif defined(ENABLE_PRIMARY_TEMP_SENSOR_DS18B20)
  #if defined(ENABLE_MINIMAL_ONEWIRE_SUPPORT)
  // DSB18B20 temperature impl, with slightly reduced precision to improve speed.
  extern OTV0P2BASE::TemperatureC16_DS18B20 TemperatureC16;
  #endif // defined(ENABLE_MINIMAL_ONEWIRE_SUPPORT)
#else // Don't use TMP112 if SHT21 or DS18B20 have been selected.
extern OTV0P2BASE::RoomTemperatureC16_TMP112 TemperatureC16;
#endif

// HUMIDITY_SENSOR_SUPPORT is defined if at least one humidity sensor has support compiled in.
// Simple implementations can assume that the sensor will be present if defined;
// more sophisticated implementations may wish to make run-time checks.
// If SHT21 support is enabled at compile-time then its humidity sensor may be used at run-time.
#if defined(ENABLE_PRIMARY_TEMP_SENSOR_SHT21)
#define HUMIDITY_SENSOR_SUPPORT // Humidity sensing available.
#endif

#if defined(ENABLE_PRIMARY_TEMP_SENSOR_SHT21)
// Singleton implementation/instance.
typedef OTV0P2BASE::HumiditySensorSHT21 RelHumidity_t;
extern RelHumidity_t RelHumidity;
#else
// Dummy implementation to minimise coding changes.
extern OTV0P2BASE::DummyHumiditySensorSHT21 RelHumidity;
#endif

#ifdef ENABLE_VOICE_SENSOR
extern OTV0P2BASE::VoiceDetectionQM1 Voice;
#endif


/////// CONTROL

// Special setup for OpenTRV beyond generic hardware setup.
void setupOpenTRV();
// Main loop for OpenTRV radiator control.
void loopOpenTRV();

// Select basic parameter set to use (or could define new set here).
#ifndef DHW_TEMPERATURES
// Settings for room TRV.
typedef OTRadValve::DEFAULT_ValveControlParameters PARAMS;
#else
// Default settings for DHW control.
typedef OTRadValve::DEFAULT_DHW_ValveControlParameters PARAMS;
#endif

// Choose which subtype to use depending on enabled settings and board type.
#if defined(TEMP_POT_AVAILABLE) // Eg REV2/REV7.
  #if defined(HUMIDITY_SENSOR_SUPPORT)
  typedef OTRadValve::TempControlTempPot<decltype(TempPot), &TempPot, PARAMS, decltype(RelHumidity), &RelHumidity> TempControl_t;
  #else
  typedef OTRadValve::TempControlTempPot<decltype(TempPot), &TempPot, PARAMS> TempControl_t;
  #endif
#elif defined(ENABLE_SETTABLE_TARGET_TEMPERATURES) // Eg REV1.
typedef OTRadValve::TempControlSimpleEEPROMBacked<PARAMS> TempControl_t;
#else
// Dummy temperature control.
typedef OTRadValve::TempControlBase TempControl_t;
#endif
#define TempControl_DEFINED
extern TempControl_t tempControl;

// Default minimum on/off time in minutes for the boiler relay.
// Set to 5 as the default valve Tx cycle is 4 mins and 5 mins is a good amount for most boilers.
// This constant is necessary as if V0P2BASE_EE_START_MIN_BOILER_ON_MINS_INV is not set, the boiler relay will never be turned on.
static const constexpr uint8_t DEFAULT_MIN_BOILER_ON_MINS = 5;
#if defined(ENABLE_DEFAULT_ALWAYS_RX)
#define getMinBoilerOnMinutes() (DEFAULT_MIN_BOILER_ON_MINS)
#elif defined(ENABLE_BOILER_HUB) || defined(ENABLE_STATS_RX)
// Get minimum on (and off) time for pointer (minutes); zero if not in hub mode.
uint8_t getMinBoilerOnMinutes();
// Set minimum on (and off) time for pointer (minutes); zero to disable hub mode.
// Suggested minimum of 4 minutes for gas combi; much longer for heat pumps for example.
void setMinBoilerOnMinutes(uint8_t mins);
#else
#define getMinBoilerOnMinutes() (0) // Always disabled.
#define setMinBoilerOnMinutes(mins) {} // Do nothing.
#endif

#if defined(ENABLE_DEFAULT_ALWAYS_RX)
// True: always in central hub/listen mode.
#define inHubMode() (true)
// True: always in stats hub/listen mode.
#define inStatsHubMode() (true)
#elif !defined(ENABLE_RADIO_RX)
// No RX/listening allowed, so never in hub mode.
// False: never in central hub/listen mode.
#define inHubMode() (false)
// False: never in stats hub/listen mode.
#define inStatsHubMode() (false)
#else
// True if in central hub/listen mode (possibly with local radiator also).
#define inHubMode() (0 != getMinBoilerOnMinutes())
// True if in stats hub/listen mode (minimum timeout).
#define inStatsHubMode() (1 == getMinBoilerOnMinutes())
#endif // defined(ENABLE_DEFAULT_ALWAYS_RX)

// Period in minutes for simple learned on-time; strictly positive (and less than 256).
#ifndef LEARNED_ON_PERIOD_M
#define LEARNED_ON_PERIOD_M 60
#endif
// Period in minutes for simple learned on-time with comfort bias; strictly positive (and less than 256).
// Defaults to twice LEARNED_ON_PERIOD_M.
// Should be no shorter/less than LEARNED_ON_PERIOD_M to avoid confusion.
#ifndef LEARNED_ON_PERIOD_COMFORT_M
#define LEARNED_ON_PERIOD_COMFORT_M (min(2*(LEARNED_ON_PERIOD_M),255))
#endif
#if defined(ENABLE_SINGLETON_SCHEDULE)
#define SCHEDULER_AVAILABLE
// Singleton scheduler instance.
typedef OTV0P2BASE::SimpleValveSchedule
    <
    LEARNED_ON_PERIOD_M, LEARNED_ON_PERIOD_COMFORT_M,
    decltype(tempControl), &tempControl,
#if defined(ENABLE_OCCUPANCY_SUPPORT)
    decltype(Occupancy), &Occupancy
#endif
    > Scheduler_t;
#else
// Dummy scheduler to simplify coding.
typedef OTV0P2BASE::NULLValveSchedule Scheduler_t;
#endif // defined(ENABLE_SINGLETON_SCHEDULE)
extern Scheduler_t Scheduler;

#if defined(ENABLE_LOCAL_TRV)
#define ENABLE_MODELLED_RAD_VALVE
#define ENABLE_NOMINAL_RAD_VALVE
// Singleton implementation for entire node.
extern OTRadValve::ModelledRadValve NominalRadValve;
#elif defined(ENABLE_SLAVE_TRV)
#define ENABLE_NOMINAL_RAD_VALVE
// Simply alias directly to FHT8V for REV9 slave for example.
#define NominalRadValve FHT8V
#endif

// Sample statistics once per hour as background to simple monitoring and adaptive behaviour.
// Call this once per hour with fullSample==true, as near the end of the hour as possible;
// this will update the non-volatile stats record for the current hour.
// Optionally call this at a small (2--10) even number of evenly-spaced number of other times throughout the hour
// with fullSample=false to sub-sample (and these may receive lower weighting or be ignored).
// (EEPROM wear should not be an issue at this update rate in normal use.)
void sampleStats(bool fullSample);

#ifdef ENABLE_FS20_ENCODING_SUPPORT
// Clear and populate core stats structure with information from this node.
// Exactly what gets filled in will depend on sensors on the node,
// and may depend on stats TX security level (if collecting some sensitive items is also expensive).
void populateCoreStats(OTV0P2BASE::FullStatsMessageCore_t *content);
#endif // ENABLE_FS20_ENCODING_SUPPORT

#ifdef ENABLE_SETBACK_LOCKOUT_COUNTDOWN // TODO Move this into OTRadioLink for mainline version.
    /**
     * @brief   Retrieve the current setback lockout value from the EEPROM.
     * @retval  The number of days left of the setback lockout. Setback lockout is disabled when this reaches 0.
     * @note    The value is stored inverted in EEPROM.
     * @note    This is stored as G 0 for TRV1.5 devices, but may change in future.
     */
    static inline uint8_t getSetbackLockout() { return ~(eeprom_read_byte((uint8_t *)OTV0P2BASE::V0P2BASE_EE_START_SETBACK_LOCKOUT_COUNTDOWN_D_INV)); }
#endif // ENABLE_SETBACK_LOCKOUT_COUNTDOWN

// Do bare stats transmission.
// Output should be filtered for items appropriate
// to current channel security and sensitivity level.
// This may be binary or JSON format.
//   * allowDoubleTX  allow double TX to increase chance of successful reception
//   * doBinary  send binary form if supported, else JSON form if supported
// Sends stats on primary radio channel 0 with possible duplicate to secondary channel.
// If sending encrypted then ID/counter fields (eg @ and + for JSON) are omitted
// as assumed supplied by security layer to remote recipent.
void bareStatsTX(bool allowDoubleTX = false, bool doBinary = false);

#ifdef ENABLE_BOILER_HUB
// Raw notification of received call for heat from remote (eg FHT8V) unit.
// This form has a 16-bit ID (eg FHT8V housecode) and percent-open value [0,100].
// Note that this may include 0 percent values for a remote unit explicitly confirming
// that is is not, or has stopped, calling for heat (eg instead of replying on a timeout).
// This is not filtered, and can be delivered at any time from RX data, from a non-ISR thread.
// Does not have to be thread-/ISR- safe.
void remoteCallForHeatRX(uint16_t id, uint8_t percentOpen);
#endif

////// UI

// Valve physical UI controller.
#if defined(ENABLE_LOCAL_TRV) && !defined(NO_UI_SUPPORT)
#define valveUI_DEFINED
  #if defined(ENABLE_SIMPLIFIED_MODE_BAKE)
  typedef OTRadValve::ModeButtonAndPotActuatorPhysicalUI valveUI_t;
  #else
  typedef OTRadValve::CycleModeAndLearnButtonsAndPotActuatorPhysicalUI<BUTTON_MODE_L> valveUI_t;
  #endif
extern valveUI_t valveUI;
#endif // ENABLE_LOCAL_TRV && !NO_UI_SUPPORT

// Check/apply the user's schedule, at least once each minute, and act on any timed events.
#if defined(SCHEDULER_AVAILABLE)
void checkUserSchedule();
#else
#define checkUserSchedule() // Reduce to a no-op.
#endif // defined(SCHEDULER_AVAILABLE)

// Sends a short 1-line CRLF-terminated status report on the serial connection (at 'standard' baud).
// Should be similar to PICAXE V0.1 output to allow the same parser to handle either.
#ifdef ENABLE_SERIAL_STATUS_REPORT
void serialStatusReport();
#else
#define serialStatusReport() { }
#endif

// Reset CLI active timer to the full whack before it goes inactive again (ie makes CLI active for a while).
// Thread-safe.
void resetCLIActiveTimer();

// Returns true if the CLI is (or should currently be) active, at least intermittently.
// Thread-safe.
bool isCLIActive();

// Suggested minimum buffer size for pollUI() to ensure maximum-sized commands can be received.
#if defined(ENABLE_EXTENDED_CLI) || defined(ENABLE_OTSECUREFRAME_ENCODING_SUPPORT)
static constexpr uint8_t MAXIMUM_CLI_RESPONSE_CHARS = 1 + OTV0P2BASE::CLI::MAX_TYPICAL_CLI_BUFFER;
#else
static constexpr uint8_t MAXIMUM_CLI_RESPONSE_CHARS = 1 + OTV0P2BASE::CLI::MIN_TYPICAL_CLI_BUFFER;
#endif
static constexpr uint8_t BUFSIZ_pollUI = 1 + MAXIMUM_CLI_RESPONSE_CHARS;

// Used to poll user side for CLI input until specified sub-cycle time.
// A period of less than (say) 500ms will be difficult for direct human response on a raw terminal.
// A period of less than (say) 100ms is not recommended to avoid possibility of overrun on long interactions.
// Times itself out after at least a minute or two of inactivity. 
// NOT RENTRANT (eg uses static state for speed and code space).
void pollCLI(uint8_t maxSCT, bool startOfMinute, char *buf, uint8_t bufsize);


////////////////////////// Actuators

// DORM1/REV7 direct drive motor actuator.
#ifdef ENABLE_PROPORTIONAL_VALVE_CONTROL
static constexpr bool binaryOnlyValveControl = false;
#else
static constexpr bool binaryOnlyValveControl = true;
#endif
#if /* defined(ENABLE_LOCAL_TRV) && */ defined(ENABLE_V1_DIRECT_MOTOR_DRIVE)
#define HAS_DORM1_VALVE_DRIVE
#ifdef ENABLE_DORM1_MOTOR_REVERSED // Reversed vs sample 2015/12.
  static constexpr uint8_t m1 = MOTOR_DRIVE_ML;
  static constexpr uint8_t m2 = MOTOR_DRIVE_MR;
#else
  static constexpr uint8_t m1 = MOTOR_DRIVE_MR;
  static constexpr uint8_t m2 = MOTOR_DRIVE_ML;
#endif // HAS_DORM1_MOTOR_REVERSED
typedef OTRadValve::ValveMotorDirectV1<m1, m2, MOTOR_DRIVE_MI_AIN, MOTOR_DRIVE_MC_AIN, decltype(Supply_cV), &Supply_cV, binaryOnlyValveControl> ValveDirect_t;
extern ValveDirect_t ValveDirect;
#endif

// Singleton FHT8V valve instance (to control remote FHT8V valve by radio).
#ifdef ENABLE_FHT8VSIMPLE
static const uint8_t _FHT8V_MAX_EXTRA_TRAILER_BYTES = (1 + max(OTV0P2BASE::MESSAGING_TRAILING_MINIMAL_STATS_PAYLOAD_BYTES, OTV0P2BASE::FullStatsMessageCore_MAX_BYTES_ON_WIRE));
extern OTRadValve::FHT8VRadValve<_FHT8V_MAX_EXTRA_TRAILER_BYTES, OTRadValve::FHT8VRadValveBase::RFM23_PREAMBLE_BYTES, OTRadValve::FHT8VRadValveBase::RFM23_PREAMBLE_BYTE> FHT8V;
#if defined(ENABLE_LOCAL_TRV) || defined(ENABLE_SLAVE_TRV)
inline bool localFHT8VTRVEnabled() { return(!FHT8V.isUnavailable()); }
#else
#define localFHT8VTRVEnabled() (false) // Local FHT8V TRV disabled.
#endif
#endif // ENABLE_FHT8VSIMPLE


#endif

