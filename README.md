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

Latest version of the AP-Nurse Care feaatures **over the air firmware update**

