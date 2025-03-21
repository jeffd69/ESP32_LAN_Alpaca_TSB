/**************************************************************************************************
  Filename:       SafetyMonitor.h
  Revised:        $Date: 2024-12-10$
  Revision:       $Revision: 01 $
  Description:    Device Alpaca SafetyMonitor V3
**************************************************************************************************/

#pragma once
#include "AlpacaSafetyMonitor.h"

#define SAFEMON_RAIN_BIT        1
#define SAFEMON_POWER_BIT       2

#define SAFEMON_TSKY_BIT        4
#define SAFEMON_WIND_BIT        8
#define SAFEMON_HUM_BIT         16
#define SAFEMON_LIGHT_BIT       32

extern u_int8_t _safemon_inputs;

extern bool is_ws_connected;
extern int16_t		weather_tsky;					              // readings from weather station
extern int16_t		weather_tair;
extern int16_t		weather_wind;
extern int16_t		weather_hum;
extern int16_t		weather_light;


class SafetyMonitor : public AlpacaSafetyMonitor
{
private:
  bool _is_safe;
  uint32_t _rain_delay;
  uint32_t _power_delay;
  uint32_t _weather_delay;
  int16_t _tsky_limit, _wind_limit, _hum_limit, _light_limit;
  bool _use_tsky, _use_wind, _use_hum, _use_light;
  uint32_t tmr_ws_sky_ini, tmr_ws_sky_len;		          // weather station timer and alarm duration
  uint32_t tmr_ws_wind_ini, tmr_ws_wind_len;

  const bool _getIsSafe();

  void AlpacaReadJson(JsonObject &root);
  void AlpacaWriteJson(JsonObject &root);

	static const char *const k_safemon_state_str[2];

public:
	SafetyMonitor();
	void Begin();
	void Loop();
  uint32_t getRainDelay() {return _rain_delay;}
  uint32_t getPowerDelay() {return _power_delay;}

};
