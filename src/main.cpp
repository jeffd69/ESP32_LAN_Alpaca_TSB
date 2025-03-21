
/**************************************************************************************************
	Filename:       main.cpp
	Revised:        Date: 2024-12-02
	Revision:       Revision: 01

	Description:    ASCOM Alpaca ESP32 TSBoard implementation
**************************************************************************************************/
//#define CONFIG_ASYNC_TCP_USE_WDT 0


// #define TEST_RESTART             // only for testing
#include "defines.h"                // pins and bitmasks
#include <ETH.h>

#include <SLog.h>
#include <AlpacaDebug.h>
#include <AlpacaServer.h>

#include <Dome.h>
#include <Switch.h>
#include <SafetyMonitor.h>

Dome domeDevice;
Switch switchDevice;
SafetyMonitor safemonDevice;

#define VERSION "1.0.0"

// ASCOM Alpaca server with discovery
AlpacaServer alpaca_server(ALPACA_MNG_SERVER_NAME, ALPACA_MNG_MANUFACTURE, ALPACA_MNG_MANUFACTURE_VERSION, ALPACA_MNG_LOCATION);

uint16_t _shift_reg_in, _shift_reg_out, _prev_shift_reg_out;
bool d_open_button, d_close_button, d_switch_opened, d_switch_closed;
bool d_relay_open, d_relay_close;

uint8_t _safemon_inputs;						// status of safety monitor 0->safe
uint32_t tmr_rain_ini, tmr_rain_len;			// timers for rain delay and alarm duration
uint32_t tmr_power_ini, tmr_power_len;			// power
//uint32_t tmr_wstat_ini, tmr_wstat_len;		// weather station

uint32_t tmr_ws_connected;						// timout for weather station connection. If elapses, ws is considered offline
bool is_ws_connected;							// true when weather station is connected
int rx_1_idx;									// index of pointed char in ws_message[]
bool rx_1_complete;								// true when a complete message is received
char rx_1_buffer[UART1_BUFFER];
char tx_1_buffer[UART1_BUFFER];
int16_t	weather_tsky;							// readings from weather station
int16_t	weather_tair;
int16_t	weather_wind;
int16_t	weather_hum;
int16_t	weather_light;
int16_t	weather_press;							// 2024-08-26 2.03 not used anymore
int16_t	weather_rain;							// 2024-08-26 2.03 added
int16_t	weather_clouds;							// 2024-08-26 2.03 added
int16_t	weather_stars;							// 2024-08-26 2.03 added

bool _sw_in[8], _sw_out[8];						// status of switch in and out
uint8_t _sw_pwm[4], _prev_sw_pwm[4];			// switch PWMs
uint8_t _sw_pwm_pins[4] = {OUT_PIN_PWM0, OUT_PIN_PWM1, OUT_PIN_PWM2, OUT_PIN_PWM3};		// definition of PWM pins

uint32_t tmr_LED, tmr_shreg_in, tmr_shreg_out;	// timers for LEDs and shift registers
uint32_t restart_start_time_ms;					// timer for restart
uint32_t const RESTART_DELAY_MS = 5000;			// restart delay

bool parse_ws_message(void);
void flush_tx(void);
void flush_rx(void);
void normal_boot(void);
uint16_t read_shift_register( void );
void write_shift_register( uint16_t value );
void init_IO(void);
void checkForRestart(void);

void setup() {
	Serial.begin(115200);
	delay(1000);
	Serial.println("Serial OK");

	init_IO();
	normal_boot();

	alpaca_server.Begin();

	domeDevice.Begin();
	alpaca_server.AddDevice(&domeDevice);

	switchDevice.Begin();
	alpaca_server.AddDevice(&switchDevice);

	safemonDevice.Begin();
	alpaca_server.AddDevice(&safemonDevice);

	alpaca_server.RegisterCallbacks();
	alpaca_server.LoadSettings();

	_shift_reg_in = 0;
	_shift_reg_out = 0;
	_prev_shift_reg_out = 0;

	tmr_LED = millis();
	tmr_shreg_in = millis();
	tmr_shreg_out = millis();

	_safemon_inputs = 0;
	tmr_rain_ini = 0; tmr_rain_len =0 ;
	tmr_power_ini = 0; tmr_power_len = 0; 
	//tmr_wstat_ini = 0; tmr_wstat_len = 0;
	is_ws_connected = false;
	rx_1_complete = false;
	rx_1_idx = 0;
	restart_start_time_ms = 0;
}

void loop()
{
	checkForRestart();

	alpaca_server.Loop();

	domeDevice.Loop();

	switchDevice.Loop();

	safemonDevice.Loop();

	if((millis() - tmr_shreg_in) > 100) {                  	// read shift register every 100ms
		tmr_shreg_in = millis();
		_shift_reg_in = read_shift_register();
	}

	if( domeDevice.GetNumberOfConnectedClients() > 0 ) {
		_shift_reg_out |= BIT_DOME;							// Dome connected LED ON

		if( _shift_reg_in & BIT_FC_CLOSE )                	// handle close switch input
			d_switch_closed = true;
		else
			d_switch_closed = false;

		if( _shift_reg_in & BIT_FC_OPEN )                 	// handle open switch input
			d_switch_opened = true;
		else 
			d_switch_opened = false;

		if( d_relay_close && !d_switch_closed ){			// handle close relay bit in the out shift register
			_shift_reg_out |= BIT_ROOF_CLOSE;
			_shift_reg_out &= ~BIT_ROOF_OPEN;
		} else {
			_shift_reg_out &= ~BIT_ROOF_CLOSE;
		}
		
		if( d_relay_open && !d_switch_opened) {				// handle open relay bit in the out shift register
			_shift_reg_out |= BIT_ROOF_OPEN;
			_shift_reg_out &= ~BIT_ROOF_CLOSE;
		} else {
			_shift_reg_out &= ~BIT_ROOF_OPEN;
		}
	} else {
		_shift_reg_out &= ~BIT_DOME;		// Dome connected LED OFF

		// set flags according to bits in shift registers
		if( _shift_reg_in & BIT_BUTTON_CLOSE)   	// if no clients connected, handle manual close button
			d_close_button = true;
		else
			d_close_button = false;
		
		if( _shift_reg_in & BIT_BUTTON_OPEN )   	// if no clients connected, handle manual open button
			d_open_button = true;
		else
			d_open_button = false;
		
		if( _shift_reg_in & BIT_FC_CLOSE )      	// handle close switch
			d_switch_closed = true;
		else
			d_switch_closed = false;

		if( _shift_reg_in & BIT_FC_OPEN )       	// handle open switch
			d_switch_opened = true;
		else 
			d_switch_opened = false;
		
		// set relays if conditions are met
		if( d_close_button && !d_open_button && !d_switch_closed ) {
			_shift_reg_out |= BIT_ROOF_CLOSE; 			// set close relay bit in the shift register
			_shift_reg_out &= ~BIT_ROOF_OPEN;			// be sure to clear OPEN RELAY bit
		}
		else if( !d_close_button && d_open_button && !d_switch_opened ) {
			_shift_reg_out |= BIT_ROOF_OPEN;   			// set open relay bit in the shift register
			_shift_reg_out &= ~BIT_ROOF_CLOSE;			// be sure to clear CLOSE RELAY bit
		} else {
			_shift_reg_out &= ~BIT_ROOF_CLOSE;			// bclear CLOSE RELAY bit
			_shift_reg_out &= ~BIT_ROOF_OPEN;			// clear OPEN RELAY bit
		}
	}

	if( safemonDevice.GetNumberOfConnectedClients() > 0 ) {
		_shift_reg_out |= BIT_SAFEMON; 									// Sefemon connected LED ON

		if(( _shift_reg_in & BIT_SAFE_RAIN ) != 0) {					// rain signal
			if( tmr_rain_ini == 0 ) {									// if it's the first event, start counting the rain delay
				tmr_rain_ini = millis();
				tmr_rain_len = 1000 * safemonDevice.getRainDelay();
			}

			if(( tmr_rain_ini != 0 ) && ((millis() - tmr_rain_ini) > tmr_rain_len))    // if alarm persists for rain_delay, set UNSAFE
				_safemon_inputs |= SAFEMON_RAIN_BIT;		
		
		} else {
			tmr_rain_ini = 0;											// clear timer and flag
			_safemon_inputs &= ~SAFEMON_RAIN_BIT;
		}

		if( safemonDevice.getPowerDelay() > 0 ) {                 		// enter only if power delay is > 0
			if(( _shift_reg_in & BIT_SAFE_POWER ) != 0) {
				if( tmr_power_ini == 0 ) {
					tmr_power_ini = millis();
					tmr_power_len = 1000 * safemonDevice.getPowerDelay();
				}

				if((tmr_power_ini != 0) && ((millis() - tmr_power_ini) > tmr_power_len))
					_safemon_inputs |= SAFEMON_POWER_BIT;				
			}
			else
			{
				tmr_power_ini = 0;
				_safemon_inputs &= ~SAFEMON_POWER_BIT;
			}
		} else {
			tmr_power_ini = 0;
			_safemon_inputs &= ~SAFEMON_POWER_BIT;
		}
	}
	else
	{
		_shift_reg_out &= ~BIT_SAFEMON;		// Sefemon connected LED OFF
		_safemon_inputs = 0;
		is_ws_connected = false;
	}

	if( switchDevice.GetNumberOfConnectedClients() > 0)
	{
		uint32_t i;
		uint16_t p;

		_shift_reg_out |= BIT_SWITCH;		// Switch connected LED ON

		for(i=0; i<8; i++)
		{
			if((_shift_reg_in & (1 << i) != 0))         // set _sw_in[] according to shift register inputs
				_sw_in[i] = true;
			else
				_sw_in[i] = false;
		
			if( _sw_out[i] )                            // set out bits according to _sw_out[] status
				_shift_reg_out |= (BIT_OUT_0 >> i);
			else
				_shift_reg_out &= ~(BIT_OUT_0 >> i);
		}

		for(i=0; i<4; i++)
		{
			if( _prev_sw_pwm[i] != _sw_pwm[i] ) {				// update pwm only if different
				_prev_sw_pwm[i] = _sw_pwm[i];

				if( p == 0) {                                   // set PWM pin to 0
					digitalWrite(_sw_pwm_pins[i], LOW);
				} else if( p == 100) {                          // set PWM pin to 1
					digitalWrite(_sw_pwm_pins[i], HIGH);
				} else {         
					p = ((uint16_t)_sw_pwm[i] * 255) / 100;		// set PWM value
					analogWrite(_sw_pwm_pins[i], (int)p);
				}
			}
		}
	} else {
		uint32_t i;

		_shift_reg_out &= ~BIT_SWITCH;						// Switch connected LED OFF

		_shift_reg_out &= BIT_OUT_CLEAR;                  	// clear all OUT bits
		for(i=0; i<8; i++)
		{
			_sw_out[i] = false;                             // clear all out
			_sw_in[i] = false;                              // set input to false
		}

		for(i=0; i<4; i++)
		{
			_sw_pwm[i] = 0;                                 // clear all PWMs
			digitalWrite(_sw_pwm_pins[i], LOW);             // set PWM pin to 0
		}
	}

	if(( millis() - tmr_LED ) < 1000 ) {                 	// blink CPU OK LED
	
		if(( millis() - tmr_LED ) < 500 )
		{
			_shift_reg_out |= BIT_CPU_OK;		// CPU LED ON
			//if( domeDevice.GetNumberOfConnectedClients() > 0) _shift_reg_out |= BIT_DOME;			// Dome connected LED ON
			//if( safemonDevice.GetNumberOfConnectedClients() > 0) _shift_reg_out |= BIT_SAFEMON; 	// Sefemon connected LED ON
			//if( switchDevice.GetNumberOfConnectedClients() > 0) _shift_reg_out |= BIT_SWITCH;		// Switch connected LED ON
		}
		else
		{
			_shift_reg_out &= ~BIT_CPU_OK;		// CPU LED OFF
			//_shift_reg_out &= ~BIT_DOME;		// Dome connected LED OFF
			//_shift_reg_out &= ~BIT_SAFEMON;		// Sefemon connected LED OFF
			//_shift_reg_out &= ~BIT_SWITCH;		// Switch connected LED OFF
		}
	} else {
		tmr_LED = millis();
	}

	if((millis() - tmr_shreg_out) > 100)                    // write shift register every 100ms
	{
		tmr_shreg_out = millis();
	
		if( _shift_reg_out != _prev_shift_reg_out )       	// write only if changed
		{
			_prev_shift_reg_out = _shift_reg_out;
			write_shift_register( _shift_reg_out );
		}
	}

	// serial from WS
	if(Serial1.available())
	{
		char in_msg = (char)Serial1.read();
		if( in_msg == '%' )                   		// frame start
			rx_1_idx = 0;
		
		rx_1_buffer[rx_1_idx++] = in_msg;     		// store char in the rx_buffer

		if(in_msg == '#') {                   		// frame end
			rx_1_buffer[rx_1_idx++] = 0;        	// append termination char

			if( rx_1_buffer[0] == '%') {
				rx_1_complete = true;
				is_ws_connected = true;
				tmr_ws_connected = millis();      	// refresh connection timer
			} else {                            	// message is incomplete, ignore
				rx_1_idx = 0;
				rx_1_buffer[rx_1_idx] = 0;
			}
		}
	}

	if (rx_1_complete) {
		parse_ws_message();
		rx_1_complete = false;
		flush_rx();
	}
}

// NEW -> decode messages from WStation and store to local variables (%WS, skytemp, airtemp, wind, humidity, rain, light, clouds, stars #)
// NEW -> typical message			%WS,-175,-120,24,85,1,1270,-1,-1#
bool parse_ws_message(void) {
	Serial.println(rx_1_buffer);

	uint8_t s, d, v;
	int16_t params[8];
	char s_val[8];

	s = 0;
	d = 0;
	v = 0;

	if((rx_1_buffer[0] == '%') && (rx_1_buffer[1] == 'W') && (rx_1_buffer[2] == 'S') && (rx_1_buffer[3] == ','))
	{
		s = 4;

		while(rx_1_buffer[s])
		{
			if(( rx_1_buffer[s] == ',' ) || ( rx_1_buffer[s] == '#' ))
			{
				params[v] = atoi( s_val );
				v++;
				s++;
				d = 0;
			}
			else
			{
				s_val[d++] = rx_1_buffer[s++];
				s_val[d] = 0;
			}
		}

		if(!(( params[0] < -500 ) || ( params[0] > 500 )))		// sky temp -500 -> 500			1adu = 0,1°C
			weather_tsky = params[0];
		
		if(!(( params[1] < -500 ) || ( params[1] > 500 )))		// air temp -500 -> 500			1adu = 0,1°C
			weather_tair = params[1];
		
		if(!(( params[2] < 0 ) || ( params[2] > 100 )))			// wind 0 -> 100				1adu = 1km/h
			weather_wind = params[2];
		
		if(!(( params[3] < 0 ) || ( params[3] > 110 )))			// humidity 0 -> 110			1adu = 1%
			weather_hum = params[3];
		
		if(!(( params[4] < 0 ) || ( params[4] > 9999 )))		// rain 0 -> 1					0 safe, 1 rain
			weather_rain = params[4];
		
		if(!(( params[5] < 0 ) || ( params[5] > 9999 )))		// light 0 -> 9999				1adu = 1lux
			weather_light = params[5];
		
		if(!(( params[6] < -1 ) || ( params[6] > 100 )))		// cloud coverage -1 -> 100		-1 not used, 0~100 percentage
			weather_clouds = params[6];
		
		if(!(( params[7] < -1 ) || ( params[7] > 9999 )))		// stars -1 -> 9999				-1 not used, 0~9999 number of stars in sight
			weather_stars = params[7];
		
		/*
		if(!(( params[5] < 0 ) || ( params[5] > 1100 )))		// pressure 0 -> 1100			1adu = 1mbar
		weather_press = params[5];
		*/
	}

	return true;
}

// flush UART buffers
void flush_tx(void) { for(int i=0; i< UART1_BUFFER; i++) tx_1_buffer[i] = 0; }
void flush_rx(void) { for(int i=0; i< UART1_BUFFER; i++) rx_1_buffer[i] = 0; }

void normal_boot() {
	// setup logging and WiFi
	g_Slog.Begin(Serial, 115200);
	SLOG_NOTICE_PRINTF("SLog started\n");
	SLOG_INFO_PRINTF("Try to connect with WiFi\n");

	WiFi.mode(WIFI_STA);
	WiFi.begin(); 			// (DEFAULT_SSID, DEFAULT_PWD);

	SLOG_INFO_PRINTF("Connecting to WiFi ..\n");
	Serial.println("Connecting to WiFi ..");
	uint16_t _attempts = 0;
	while ((WiFi.status() != WL_CONNECTED) && (_attempts < 60))
	{
		delay(1000);
		_attempts++;
		Serial.print(".");

		if(!(_attempts < 60)) {
			ESP.restart();
		}
	}
	
	IPAddress ip = WiFi.localIP();
	char wifi_ipstr[32]; // = "xxx.yyy.zzz.www";
	snprintf(wifi_ipstr, sizeof(wifi_ipstr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	SLOG_INFO_PRINTF("connected with %s\n", wifi_ipstr);
	Serial.printf("connected with %s\n", wifi_ipstr);
	
	// finalize logging setup
	g_Slog.Begin(alpaca_server.GetSyslogHost().c_str());
	SLOG_INFO_PRINTF("SYSLOG enabled and running log_lvl=%s enable_serial=%s\n", g_Slog.GetLvlMskStr().c_str(), alpaca_server.GetSerialLog() ? "true" : "false"); 
	g_Slog.SetLvlMsk(alpaca_server.GetLogLvl());
	g_Slog.SetEnableSerial(alpaca_server.GetSerialLog());
}

// read inputs from shift register 165, returns uint16_t value
uint16_t read_shift_register( void )
{
	uint16_t v = 0;

	digitalWrite(SR_IN_PIN_CP, LOW);    	// be sure CP is low
	digitalWrite(SR_IN_PIN_PL, LOW);    	// latch parallel inputs
	delayMicroseconds(1);
	digitalWrite(SR_IN_PIN_PL, HIGH);
	delayMicroseconds(1);
	digitalWrite(SR_IN_PIN_CE, LOW);    	// on CE -> low, D7 is available on serial out Q7
	delayMicroseconds(1);

	for(uint16_t i=0; i<16; i++) {
		v = (v << 1);
		v += digitalRead(SR_IN_PIN_SDIN);

		digitalWrite(SR_IN_PIN_CP, HIGH);   // shift to the left
		delayMicroseconds(1);
		digitalWrite(SR_IN_PIN_CP, LOW);
		delayMicroseconds(1);
	}

	digitalWrite(SR_IN_PIN_CE, HIGH);

	return ((~v) & 0x3fff);
}

// put value on the shift registers 595
void write_shift_register( uint16_t value )
{
	uint16_t v = value;

	digitalWrite(SR_OUT_PIN_SDOUT, LOW);        // 14 serial data low
	digitalWrite(SR_OUT_PIN_MR, LOW);           // 10 clear previous data
	delayMicroseconds(1);
	digitalWrite(SR_OUT_PIN_SHCP, HIGH);        // 11 shift register clock
	delayMicroseconds(1);
	digitalWrite(SR_OUT_PIN_SHCP, LOW);
	delayMicroseconds(1);
	digitalWrite(SR_OUT_PIN_MR, HIGH);
	delayMicroseconds(1);

	for(uint8_t i = 0; i < 16; i++) {
		if((v & 0x8000) == 0 )
			digitalWrite(SR_OUT_PIN_SDOUT, LOW);
		else
			digitalWrite(SR_OUT_PIN_SDOUT, HIGH);

		delayMicroseconds(1);
		digitalWrite(SR_OUT_PIN_SHCP, HIGH);
		delayMicroseconds(1);
		digitalWrite(SR_OUT_PIN_SHCP, LOW);

		v = (v << 1);
	}

	delayMicroseconds(1);
	digitalWrite(SR_OUT_PIN_STCP, HIGH);        // transfer serial data to parallel output
	delayMicroseconds(1);
	digitalWrite(SR_OUT_PIN_STCP, LOW);         // 12
	digitalWrite(SR_OUT_PIN_OE, LOW);           // 13 enable output
}

// initialize IOs and pin status
void init_IO( void ) {
	pinMode(SR_OUT_PIN_OE, OUTPUT);             // output enable
	pinMode(SR_OUT_PIN_STCP, OUTPUT);           // storage clock pulse
	pinMode(SR_OUT_PIN_MR, OUTPUT);             // master reset
	pinMode(SR_OUT_PIN_SHCP, OUTPUT);           // shift register clock pulse
	pinMode(SR_OUT_PIN_SDOUT, OUTPUT);          // serial data out

	pinMode(OUT_PIN_PWM0, OUTPUT);
	pinMode(OUT_PIN_PWM1, OUTPUT);
	pinMode(OUT_PIN_PWM2, OUTPUT);
	pinMode(OUT_PIN_PWM3, OUTPUT);

	pinMode(SR_IN_PIN_CE, OUTPUT);              // chip enable
	pinMode(SR_IN_PIN_CP, OUTPUT);              // clock pulse
	pinMode(SR_IN_PIN_PL, OUTPUT);              // parallel latch
	pinMode(SR_IN_PIN_SDIN, INPUT);             // serial data in

	pinMode(IN_PIN_AP_SET, INPUT);      		// net configuration button
	pinMode(OUT_PIN_AP_LED, OUTPUT);           	// net configuration LED
	
	digitalWrite(SR_OUT_PIN_OE, LOW);			// shift register out
	digitalWrite(SR_OUT_PIN_STCP, LOW);
	digitalWrite(SR_OUT_PIN_MR, LOW);
	digitalWrite(SR_OUT_PIN_SHCP, LOW);
	digitalWrite(SR_OUT_PIN_SDOUT, LOW);

	digitalWrite(OUT_PIN_PWM0, LOW);
	digitalWrite(OUT_PIN_PWM1, LOW);
	digitalWrite(OUT_PIN_PWM2, LOW);
	digitalWrite(OUT_PIN_PWM3, LOW);

	digitalWrite(SR_IN_PIN_CE, HIGH);
	digitalWrite(SR_IN_PIN_CP, LOW);
	digitalWrite(SR_IN_PIN_PL, HIGH);

	digitalWrite(OUT_PIN_AP_LED, LOW);

	// clock pulse on 74HC595 shift register with OE and MR low
	digitalWrite(SR_OUT_PIN_SHCP, HIGH);
	usleep(10);
	digitalWrite(SR_OUT_PIN_STCP, HIGH);
	usleep(10);
	digitalWrite(SR_OUT_PIN_SHCP, LOW);
	usleep(10);
	digitalWrite(SR_OUT_PIN_STCP, LOW);
	usleep(10);
	digitalWrite(SR_OUT_PIN_MR, HIGH);

	analogWriteFrequency(1000);         // set PWM 1KHz 8bits
	analogWriteResolution(8);

	Serial1.begin(9600, SERIAL_8N1, IN_PIN_RX1, OUT_PIN_TX1);
}

// restart ESP32 on 192.168.1.123/reset page
void checkForRestart(void) {
	if ( alpaca_server.GetResetRequest() ) {
		if( restart_start_time_ms == 0 ) {
			restart_start_time_ms = millis();
			Serial.println("Restart request");
		}

		if(( millis() - restart_start_time_ms ) > RESTART_DELAY_MS ) {
			Serial.println("Restarting");
			delay(1000);
			ESP.restart();
		}
	}
}


