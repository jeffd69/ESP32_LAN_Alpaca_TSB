/* *************************************************************************************************
  Filename:       defines.h
  Revised:        $Date: 2024-12-12$
  Revision:       $Revision: 01 $
  Description:    board definitions
  
************************************************************************************************* */
/*///////////////////////////////////////////////
CONFIGURATION NEEDED BY LAN8720 CHIP DO NOT EDIT!
///////////////////////////////////////////////*/
#define ETH_CLOCK_IN_PIN 		0
#define ETH_MDIO_PIN 				18
#define ETH_TXD0_PIN 				19
#define ETH_TXEN_PIN 				21
#define ETH_TXD1_PIN 				22
#define ETH_MDC_PIN 				23
#define ETH_RXD0_PIN 				25
#define ETH_RXD1_PIN 				26
#define ETH_MODE2_PIN 			27
#define ETH_POWER_PIN 			17

#define ETH_ADDR 						1
#define ETH_TYPE 						ETH_PHY_LAN8720
#define ETH_CLK_MODE 				ETH_CLOCK_GPIO0_IN

#define SYSLOG_HOST         "0.0.0.0"   // your SysLog-Host

#define SR_OUT_PIN_OE       15          // 595 shift register output enable
#define SR_OUT_PIN_STCP     2           // output latch storage clock
#define SR_OUT_PIN_MR       12          // shift register master reset
#define SR_OUT_PIN_SHCP     14          // shift register serial clock
#define SR_OUT_PIN_SDOUT    27          // serial data out

#define OUT_PIN_PWM0        32          // PWM channels
#define OUT_PIN_PWM1        33
#define OUT_PIN_PWM2        25
#define OUT_PIN_PWM3        26

#define SR_IN_PIN_CE        5           // 165 shift register chip enable
#define SR_IN_PIN_CP        18          // clock
#define SR_IN_PIN_PL        19          // parallel load
#define SR_IN_PIN_SDIN      4           // serial data in

#define IN_PIN_AP_SET       34          // net config button pin
#define OUT_PIN_AP_LED      13          // net config LED

#define WS_TIMEOUT          30          // 30s timeout if no data received from WS
#define IN_PIN_RX1          16          // usart RX from weather station
#define OUT_PIN_TX1         17          // usart TX to weather station
#define UART1_BUFFER        64          // size of uart buffers

// bit mask for output shif register 595
#define BIT_OUT_CLEAR       0xff00      // 0b1111 1111 0000 0000

#define BIT_OUT_0           0x0080      // 0bx000 0000 1000 0000
#define BIT_OUT_1           0x0040      // 0bx000 0000 0100 0000
#define BIT_OUT_2           0x0020      // 0bx000 0000 0010 0000
#define BIT_OUT_3           0x0010      // 0bx000 0000 0001 0000
#define BIT_OUT_4           0x0008      // 0bx000 0000 0000 1000
#define BIT_OUT_5           0x0004      // 0bx000 0000 0000 0100
#define BIT_OUT_6           0x0002      // 0bx000 0000 0000 0010
#define BIT_OUT_7           0x0001      // 0bx000 0000 0000 0001

#define BIT_ROOF_CLOSE      0x0100      // bx000 0001 0000 0000
#define BIT_ROOF_OPEN       0x0200      // bx000 0010 0000 0000
#define BIT_CPU_OK          0x0400      // b0000 0100 0000 0000
#define BIT_WS_OK           0x0800      // b0000 1000 0000 0000
#define BIT_DOME            0x1000      // b0001 0000 0000 0000
#define BIT_SWITCH          0x2000      // b0010 0000 0000 0000
#define BIT_SAFEMON         0x4000      // b0100 0000 0000 0000


// bit mask for input shif register 165
#define BIT_IN_0            0x0001      // bxx00 0000 0000 0001
#define BIT_IN_1            0x0002      // bxx00 0000 0000 0010
#define BIT_IN_2            0x0004      // bxx00 0000 0000 0100
#define BIT_IN_3            0x0008      // bxx00 0000 0000 1000
#define BIT_IN_4            0x0010      // bxx00 0000 0001 0000
#define BIT_IN_5            0x0020      // bxx00 0000 0010 0000
#define BIT_IN_6            0x0040      // bxx00 0000 0100 0000
#define BIT_IN_7            0x0080      // bxx00 0000 1000 0000

#define BIT_FC_CLOSE        0x0100      // bx000 0001 0000 0000
#define BIT_FC_OPEN         0x0200      // bx000 0010 0000 0000
#define BIT_BUTTON_OPEN     0x0400      // bx000 0100 0000 0000
#define BIT_BUTTON_CLOSE    0x0800      // bx000 1000 0000 0000

#define BIT_SAFE_RAIN       0x1000      // bxx01 0000 0000 0000
#define BIT_SAFE_POWER      0x2000      // bxx10 0000 0000 0000
