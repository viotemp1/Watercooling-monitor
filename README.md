# Watercooling-monitor
PC Watercooling monitor with Sparkfun MicroView and Adafruit AMG8833 8x8 Thermal Camera Sensor

Wiring:

MicroView USB
2	-	SCL (A5)
3	-	SDA	 (A4)
8	-	GND
11	-	INT0
15	-	V+

AMG8833
V+		-	to 15
3V		-	
GND	  -	to 8
SDA		-	to 3
SCL		-	to 2
INT		-	to 11

AMG8833 will monitor the liquid temperature (should me mounted on the highest part of cooling system (transparent resevoir or tube/pipe) just bellow the air/liquid line.
It will report temperature difference between liquid and air. 
When this is bellow a limit (-1 in my case) it can trigger an alarm (optional buzzer). This can happen when there are leaks and the water level drop bellow sensor.
Readings (delta temp) can be also monitored with external script sendinf commands periodically via serial port.


[Monitoring](https://github.com/viotemp1/Watercooling-monitor/blob/main/Screenshot.png)
