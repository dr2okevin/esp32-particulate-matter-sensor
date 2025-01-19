# ESP32 Particulate matter sensor
This little code reads out an SDS011 sensor, and publishes the Data via mqtt

SDS011 is widely available on any maker store, Aliexpress and Amazon.  
I bought this one from amazon https://amzn.eu/d/2LMqJiG

set you wifi settings in line 17 and 18
and maybe you want to change the mqtt settings in line 26 to 29

the SDS011 pins are set on line 32 and 33

I used the Arduino IDE 1.8 for this.

Following Libaries where used
- Sds011 https://github.com/dok-net/esp_sds011/tree/1.1.0
- EspSoftwareSerial https://github.com/plerup/espsoftwareserial/tree/6.17.1
- WiFi https://github.com/arduino-libraries/WiFi/tree/1.2.7
- time
- PubSubClient https://github.com/knolleary/pubsubclient/tree/v2.8
