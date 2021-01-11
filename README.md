# AP-Care-Loggers

## Building and flashing

**Before building and flashing replace authCredenitals.h with the correct config file!**

Version selection is handled by the following directives:

```C++
/** Version selection
 *
 *  @note select only one
 */
#define _HALLWAY
//#define _DOOR
//#define _BED
//#define _KITCHEN
```

In case the HAT version of the PIR sensor is used (the one on top of the device) note the following directive

```
/** PIR sensor selection
 *
 *  @note if the HAT version of the sensor is used, uncomment the directive, leave commented otherwise
 */
//#define _PIR_HAT
```

## FTDI

* Flashing using FTDI (USB to UART) is handled in the same manner as the Home version
* **Target: M5StickC**

## OTA

Latest version of the AP-Nurse Care features **over the air firmware update**

**Before OTA FW update, device need to be provisioned and connected to a WiFi network!**

1. Connect your computer to the same network as the target device, or the othe way around

2. 

# WiFi provisioning

AP-Nurse Care features WiFi credentials provisioning using a webserver running on a SoftAP WiFi acces point.

To connect your device to a WiFi network:

1. Turn on device

Device will check if it has valid wifi credentials and will try to connect to a network, on connection fail the device creates an acces point with the **nice-AP** SSID, where it will start a web serverwaiting for wifi credentials provisioning

2. Connect to the nice-AP AP

*Using a smartphone is recommended*

![Image of Connect to SoftAP](https://i.ibb.co/tM8M8v1/Screenshot-20210111-110427.png = 250x)

3. Using any HTTP 1.1 compatible browser navigate to **192.168.1.4**

![Image of goto 192.169.4.1](https://i.ibb.co/KNKmjpL/Screenshot-20210111-111307.png = 250x)

4. Select Wi-Fi settings

![Image of Click WiFi Settings](https://i.ibb.co/4Wf7frx/Screenshot-20210111-110445.png = 250x)

5. Setup Wi-Fi crerdentials and submit

![Image of WiFi setup](https://i.ibb.co/0Vz9tmC/Screenshot-20200902-132443.png = 250x)

**On succesful setup ull see this**

![Image of Success](https://i.ibb.co/K94yC1R/Screenshot-20210111-110541.png = 250x)
