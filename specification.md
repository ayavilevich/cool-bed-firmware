# Cool Bed project

## Introduction

This is firmware for a bed cooling system. The firmware will control a hardware device that circulates water, by using a pump, from a container through a special mattress with tubes.
The goal of the device is to cool the person sleeping on the mattress.
A second, cooling, container has cold water and ice packs. This container is preferably an ice box. A second, cooling, pump can circulate cold water to cool the main container to a desired temperature.
This method will be referred to as the "Two Buckets" method.
Optionally, a heating element can be present in the main container to heat the water as well, though this is not the main goal of the device.

The firmware has a secondary method of operation that uses variable circulation pump speed to control water temperature. This method is experimental. 
Normally it is not enabled and can be enabled using the METHOD_VARIABLE_CIRCULATION_SPEED define. If using the variable speed mode it is strongly recommended to have a flow sensor to detect and prevent stall. 
The variable speed method requires a pump that can be driven well with a PWM power source.

### Hardware

* A water container with ambient temperature water. Called the circulation container.
* An ice box with cold water and ice packs. Called the cooling container.
* A submerged circulation water pump (like one used in an aquarium).
* A submerged cooling water pump.
* An optional submerged water heating element (like one used in an aquarium).
* An outgoing water temperature sensor (Dallas, One Wire based) that monitors temperature in the circulation container.
* An optional returning water temperature sensor (Dallas, One Wire based) that monitors temperature of water returning from the mattress.
* An optional cooling water temperature sensor (Dallas, One Wire based) that monitors temperature of water in the cooling container.
* An optional water flow sensor (that sends digital pulses out) which is installed at the water return point to the circulation container.
* A custom PCB for the project or an ESP32 dev board.
* An ESP32 micro-controller to run the device.
* MOSFET modules to drive the pumps and the heating element. Prototype uses TB6612, DRV8871 and an isolated MOSFET.
* An optional current and voltage measuring IC (INA226) for each output. Later add support for INA3221.
* An optional red error LED.
* An optional green indicator LED.
* A built-in ESP32 LED (IO2).
* A built-in ESP32 button (IO0).
* (not relevant for firmware) a buck converter to convert input DC voltage to 5V for electronics.
* (not relevant for firmware) a PSU with enough capacity to power all components (usually 9V3A with heating and 9V1A without heating). The voltage of the PSU should be in range of the pumps' accepted voltage.

Use built-in LED as indicator of Wi-Fi connectivity to an AP. Blink LED slowly when not connected or connecting. On when connected.
Use green LED as indicator of pump use. Solid when the circulation pump is running, flashing when cooling is running (assume circulation is always on when cooling).
Use red LED for error state as described below. Solid when error. When there is no error, flash when heating is running.

For INA226, use INA226_AVERAGE_128, INA226_CONV_TIME_204 settings.
For INA circulation and cooling outputs set resistor = 0.1 Ohm and gain range up to 1.2A.
When INA is used with a heating element output, use a different resistor value of 0.02 Ohm for a higher current range of 6A.
All consts in the relevant source code file.
Support INA226 today and INA3221 in the future.

#### Pins

To be defined in the relevant env in platformio.ini

Here are the values for Cool Bed Prototype V1 board:

	-D BUTTON_PIN=0
	-D DALLAS_SENSOR_RETURNING_PIN=5
	-D DALLAS_SENSOR_OUTGOING_PIN=18
	-D DALLAS_SENSOR_COOLING_PIN=19
	-D CIRCULATION_PWM_PIN=27
	-D CIRCULATION_TB6612_AIN1_PIN=12
	-D CIRCULATION_TB6612_AIN2_PIN=13
	-D CIRCULATION_TB6612_STBY_PIN=14
	-D COOLING_PWM_PIN=33
	-D HEATING_PWM_PIN=23
	-D RED_LED_PIN=26
	-D GREEN_LED_PIN=25
	-D BUILTIN_LED_PIN=2
	-D FLOW_SENSOR_PIN=15

CIRCULATION_TB6612_\* defines are to be present if TB6612 is to be used. Otherwise CIRCULATION_PWM_PIN is used for a generic PWM (motor) driver.

Some parts are optional and the device should work except for the functionality provided by that part. Optional parts include: heating element, returning temperature sensor, cooling temperature sensor, flow sensor, current/voltage measurement. The cooling pump is technically optional but in practice key to proper cooling performance.

If some mandatory sensor is not defined using defines or a particular combination of defines is against the logic, then cause a compile error.
If some mandatory sensor is defined but fails during run time, then that would cause an error state. See logic below for retrying temperature sensors, etc.

Default ESP32 I2C pins 21 and 22.

Button on IO0 can't be used at the moment of the reset but can be used once the device is booting. If button is held for 5 seconds (const) after boot, reset Wi-Fi credentials via WiFiManager and let it load the WiFiManager's UI for the user to select (another) Wi-Fi once again.

#### I2C Addresses

Default INA address is 0x40 (64).

Optional current measurement features are defined by these defines:
	-D INA226_CIRCULATION_ADDRESS=0x40
	-D INA226_COOLING_ADDRESS=0x41
	-D INA226_HEATING_ADDRESS=0x42
or
	-D INA3221_ADDRESS=0x40
	-D INA3221_CIRCULATION_CHANNEL=0
	-D INA3221_COOLING_CHANNEL=1
	-D INA3221_HEATING_CHANNEL=2

### Technology stack

* Platform.io project using the Arduino framework
* Client side web parts using Alpine.js (https://alpinejs.dev/ and https://github.com/alpinejs/alpine)
* Graphs and charts for client side using Chart.js (https://www.chartjs.org/ and https://github.com/chartjs/Chart.js)
* Use SPIFFS for filesystem to store static web files
* Server side web parts using ESPAsyncWebServer, AsyncTCP and ArduinoJson
* WiFi network provisioning using WifiManager (https://github.com/tzapu/WiFiManager)
* After device is on the network, initial configuration is over the web interface
* https://github.com/wollewald/INA226_WE for INA226
* https://github.com/PaulStoffregen/OneWire and https://github.com/milesburton/Arduino-Temperature-Control-Library for Dallas temperature
* https://github.com/sparkfun/SparkFun_TB6612FNG_Arduino_Library for TB6612

### Project general coding guidelines

* Prefer modern JavaScript (ES6+) features like const/let, arrow functions, and template literals  
* Use PascalCase for component names, interfaces, and type aliases  
* Use camelCase for variables, functions, and methods  
* Prefix private class members with underscore (\_)  
* Use ALL\_CAPS for constants  
* Use meaningful variable and function names that clearly describe their purpose  
* Include helpful comments for complex logic  
* Add error handling for user inputs and API calls  
* Separate individual modules to separate source files/classes  
* Use tabs for indentation
* In C/C++ code, use #define for any constants, strings, etc

## Components

* State - where the state of the device is stored
* Connectivity - uses WiFiManager to configure Wi-Fi and then maintains Wi-Fi connection
* Controller - manages the hardware
* Web Interface - allows the user to interact with the system using a web page
* MQTT Interface - allow the user to interact with the system using MQTT

The various components can run in their own thread or can be invoked periodically from the main loop.

### State

Keep latest sensor readings and the configuration.
Persist configuration to ESP32 "Preferences" on change and load persisted values on start.
Protect state by a semaphore/mutex.

When serializing state for mqtt or web, only serialize the configuration and metrics that are valid per current hardware configuration.
Have a function in the State to validate a mode value to one of the values that are valid per hardware parts that are present.

Create a templated class to facilitate metrics/variables. One instance of the class will be created for each variable. The class will keep the value, implement serializing and parsing to String, serializing and parsing to json value, min/max values, units, description and perform other common functionality that all configuration properties have.

Create a templated class to facilitate configuration variables (inherits from the "metric" class above). One instance of the class will be created for each configuration variable. The class will allow loading and saving to "Preferences" and perform other common functionality that all configuration variables have.
Based on template type, decide Preference type, etc. Use a second shorter key for Preferences that is no more than 16 chars.

Here are the variables of the state that are set by the user and we need to persist (configuration):

| Name                | type   | default | meaning                                             | values                                | web interface     | mqtt interface |
| :----               | :----  | :----   | :----                                               | :----                                 | :----             | :----          |
| hostname            | string | cool-bed| hostname (also id) of the device                    | [a-zA-Z0-9_-]                         | input.text        | Not available  |
| mqtt                | bool   | false   | whether to enable MQTT                              | true/false                            | input.checkbox    | Not available  |
| mqttServer          | string |         | hostname of the mqtt server                         | any string                            | input.text        | Not available  |
| mqttPort            | int    | 1883    | port of the mqtt server (not TLS supported)         | valid port number                     | input.number      | Not available  |
| mqttUsername        | string |         | username for the mqtt server                        | any string                            | input.text        | Not available  |
| mqttPassword        | string |         | password for the mqtt server                        | any string                            | input.password    | Not available  |
| mqttRootTopic       | string | cool-bed| where to publish and subscribe in the hierarchy     | any string                            | input.text        | Not available  |
| mqttHADiscovery     | bool   | true    | whether to enable HA MQTT discovery                 | true/false                            | input.checkbox    | Not available  |
| mqttHADiscoveryTopic|string  | homeassistant | match HA configuration for discovery          | any string                            | input.text        | Not available  |
| mode                | string | stop    | how to operate, enum                                | see below                             | individual buttons| Select         |
| circSpeedSetPoint   | byte   | 255     | at which speed to operate the circulation pump      | [1, 255]                              | slider            | Number         |
| coolingSpeedSetPoint| byte   | 255     | at which speed to operate the cooling pump          | [0, 255]                              | slider            | Number         |
| heatingSpeedSetPoint| byte   | 255     | at which speed to operate the heating element       | [0, 255]                              | slider            | Number         |
| temperatureSetPoint | float  | 26      | temperature set point in mode=temperature           | [10, 40]                              | slider            | Number         |
| calibrationVolume   | uint   | 500     | volume in ml that was used to calibrate flow        | [1, 5000]                             | input.number      | Number         |
| calibrationFlowPulses| uint  | 500     | filtered flow pulses measured while calibrating flow| [1, 10000]                            | input.number      | Number         |
| systemTime          | uint   | 30      | seconds to respond to a change in input             | [0, 600]                              | input.number      | Number         |
| minFlowPulsesPerSec | uint   | 5       | minimum filtered flow pulses per sec to consider flow | [1, 1000]                           | input.number      | Number         |
| maxCircCurrent      | uint   | 700     | mA of circ pump current after which to trigger error| [1, 1200]                             | input.number      | Number         |
| minCircCurrent      | uint   | 200     | mA of circ pump current below which to trigger error| [1, 1200]                             | input.number      | Number         |
| minCircVoltage      | uint   | 4000    | mV of circ pump voltage below which to trigger error| [0, 40000]                            | input.number      | Number         |
| maxCoolingCurrent   | uint   | 700     | mA of cooling current after which to trigger error  | [1, 1200]                             | input.number      | Number         |
| minCoolingCurrent   | uint   | 200     | mA of cooling current below which to trigger error  | [1, 1200]                             | input.number      | Number         |
| minCoolingVoltage   | uint   | 4000    | mV of cooling voltage below which to trigger error  | [0, 40000]                            | input.number      | Number         |
| maxHeatingCurrent   | uint   | 2500    | mA of heating current after which to trigger error  | [1, 4000]                             | input.number      | Number         |
| minHeatingCurrent   | uint   | 200     | mA of heating current below which to trigger error  | [1, 4000]                             | input.number      | Number         |
| minHeatingVoltage   | uint   | 4000    | mV of heating voltage below which to trigger error  | [0, 40000]                            | input.number      | Number         |
| outTemperatureCalibrationOffset    | float  | 0       | value to add to outgoing temperature sensor reading| [-10, 10]               | input.number      | Number         |
| returnTemperatureCalibrationOffset | float  | 0       | value to add to return temperature sensor reading  | [-10, 10]               | input.number      | Number         |
| coolingTemperatureCalibrationOffset| float  | 0       | value to add to cooling water temperature sensor reading  | [-10, 10]        | input.number      | Number         |

Mode enum options: stop, temperature, manual_circ, manual_cool, manual_heat, flow_calibration, flow_test. Only show options and allow to set them if the required optional hardware is present. See explanations of the modes below.

Note: calibrationFlowPulses is user-editable config but also can be set by the firmware. This is to allow tuning by the user. Assume most of the time they will be automatically calculated.

Here are the variables of the state that are metrics from the device which we need to collect and report (telemetry)
Some will be missing if the source of the data is not available as set by the various defines. The state of what is available and what is not is reflected in the xPresent telemetry metrics.

| Name               | type   | default | meaning                                         | values         | web interface     | mqtt interface |
| :----              | :----  | :----   | :----                                           | :----          | :----             | :----          |
| status             | string |         | what the device is currently doing              | any string     | div/span          | Sensor         |
| error              | bool   | false   | are we in error state right now?                | true/false     | icon              | Binary sensor  |
| fwVersion          | string | FW_VERSION | firmware version from FW_VERSION build flag in platformio.ini | any string | div/span | Sensor     |
| rssi               | int    | -127    | Wi-Fi signal strength                           | [-127, 0] dBm  | div/span          | Sensor         |
| buildDateTime      | string | __DATE__ __TIME__ | firmware compile date and time        | any string     | div/span          | Sensor         |
| buildTimestamp     | string | __TIMESTAMP__ | firmware compile timestamp                | any string     | div/span          | Sensor         |
| circSpeed          | byte   | 0       | at which speed the circ pump is operated now    | [0, 255]       | div/span          | Sensor         |
| coolingSpeed       | byte   | 0       | at which speed the cooling pump is operated now | [0, 255]       | div/span          | Sensor         |
| heatingSpeed       | byte   | 0       | at which speed the heating element is operated  | [0, 255]       | div/span          | Sensor         |
| flowPulsesRaw      | uint64 | 0       | flow pulses (absolute, measured by interrupt)   | any integer    | div/span          | Sensor         |
| flowPulsesFiltered | uint64 | 0       | flow pulses (absolute, measured by sampling)    | any integer    | div/span          | Sensor         |
| circVoltage        | uint   | 0       | circulation pump voltage in mV                  | any integer    | div/span          | Sensor         |
| circCurrent        | int    | 0       | circulation pump current in mA                  | any integer    | div/span          | Sensor         |
| coolingVoltage     | uint   | 0       | cooling pump voltage in mV                      | any integer    | div/span          | Sensor         |
| coolingCurrent     | int    | 0       | cooling pump current in mA                      | any integer    | div/span          | Sensor         |
| heatingVoltage     | uint   | 0       | heating element voltage in mV                   | any integer    | div/span          | Sensor         |
| heatingCurrent     | int    | 0       | heating element current in mA                   | any integer    | div/span          | Sensor         |
| outTemperature     | float  | 20      | outgoing water temperature in C                 | [-5, 40]       | div/span          | Sensor         |
| returnTemperature  | float  | 20      | returning water temperature in C                | [-5, 40]       | div/span          | Sensor         |
| coolingTemperature | float  | 20      | cooling water temperature in C                  | [-5, 40]       | div/span          | Sensor         |
| flowSensorPresent  | bool   | false   | is there a flow sensor to measure flow          | true/false     | icon              | Binary sensor  |
| coolingPresent     | bool   | false   | is there a cooling pump present                 | true/false     | icon              | Binary sensor  |
| heatingPresent     | bool   | false   | is there a heating element present              | true/false     | icon              | Binary sensor  |
| coolingTemperaturePresent| bool| false| is there a temp sensor for the cooling container| true/false     | icon              | Binary sensor  |
| returnTemperaturePresent | bool| false| is there a temp sensor for the return water     | true/false     | icon              | Binary sensor  |
| circIVPresent      | bool   | false   | is there a current/voltage sensor for circulati.| true/false     | icon              | Binary sensor  |
| coolingIVPresent   | bool   | false   | is there a current/voltage sensor for cooling   | true/false     | icon              | Binary sensor  |
| heatingIVPresent   | bool   | false   | is there a current/voltage sensor for heating   | true/false     | icon              | Binary sensor  |

Here are some calculated metrics that are based on other metrics (more telemetry)
Some will be missing if the source of the data is not available as set by the various defines.

| Name                     | type   | default | meaning                                              | values       | web interface     | mqtt interface |
| :----                    | :----  | :----   | :----                                                | :----        | :----             | :----          |
| flowPulsesRawPerSec      | uint   | 0       | flow pulses per second (based on flowPulsesRaw)      | any integer  | div/span          | Sensor         |
| flowPulsesFilteredPerSec | uint   | 0       | flow pulses per second (based on flowPulsesFiltered) | any integer  | div/span          | Sensor         |
| flow                     | float  | 0       | flow in liters/min                                   | [0, 100]     | div/span          | Sensor         |
| temperatureDelta         | float  | 0       | change in temperature in deg C. return - out         | [-40, 40]    | div/span          | Sensor         |
| coolingPower             | float  | 0       | in watt. Δt * 4186 jauls * flow / 60                 | any float    | div/span          | Sensor         |

A type of "uint" refers to an "unsigned int" C/C++ type.

Note:
When calculating metrics such as flowPulsesRawPerSec, keep the prev value and the prev millis(). Then subtract the two counters (this should handle overflow if any) and adjust for 1000ms.

Note:
ml per pulse = calibrationVolume / calibrationFlowPulses
flow (L/min) = (flowPulsesFilteredPerSec × calibrationVolume × 60) / (calibrationFlowPulses × 1000)

### Connectivity

If there is no saved Wi-Fi network to connect to, use WiFiManager library per its default mode of operation. Use WiFiManager AP SSID = hostname variable value.
Once a network was configured, connect to that network.
Reconnect to the network if the connection is lost.
Register hostname.local with mDNS.
Reset of current Wi-Fi settings is done by a long button press as described above.

### Controller

Start in stop mode. This way any outputs are off at first.

Sample sensors and update telemetry.
Operate pump(s) and heating per specified mode.
Error state is when a red LED is lit, outputs are stopped and state reports an error (preferably with cause).
When mode is changed by the user, reset error state (LED, etc).
If current/voltage sensing is present for an output:
* At any time when xCurrent > maxXCurrent or xVoltage < minXVoltage, trigger an error state.
* At any time when xCurrent < minXCurrent and "x" is active for more than OUTPUT_RAMP_UP_MS (const, 1000ms), trigger an error state.
* At any time when INA overflows or reports an error, trigger an error state.

It can take 30 seconds for the change in circulation pump to affect the returning temperature and/or flow. Plan accordingly and use systemTime in the logic.

#### Sensors

Sample sensors every second.
If a flow meter is present: See difference in pulses from last sample. Calculate derived telemetry based on difference and calibration settings.
For mandatory temperature sensor(s): If error reading sensor, or if returned value is invalid, retry up to 3 (const) times and trigger an error state.
For optional temperature sensor(s): Try to read once. If missing or invalid, mark as not present, do not trigger error state.
Valid sensor temperature range is -5 to 40 degrees celsius (consts).
In the variable speed method, the outgoing and returning temperature sensors are mandatory.
In the two bucket method, only the outgoing temperature sensor is mandatory.

##### Flow meter sensor

flowPulsesRaw is the count of RISING interrupts on the pin. Could have noise on some systems.
flowPulsesFiltered is more complex. Implement in a separate class. Maintain a uint16 pin history state, current bool state and up and down counters. Setup a timer to fire every 50 microseconds (constant). In the timer interrupt, read the level of the pin with gpio_get_level. Push the pin level in the history as a bit (FIFO). This way the history keeps the latest 16 levels of the pin. When history changes to UINT16_MAX or 0 derive new state. If new state different from current state then inc up or down counter and update current state. Init history per gpio_get_level value at init time.

The flow meter sensor is optional.

#### Operation modes

##### Stop mode (stop)
Outputs are stopped. If a user moved us to this mode then a status = "Stopped".
If returnTemperature sensor is present and after 10 minutes (const) of being stopped, absolute deltaTemperature value is over 0.2 deg C (const) then set status to a warning text:
"Warning: temperature sensors might not be calibrated"

##### Manual circulation mode (manual_circ)
Operate circulation pump at circSpeedSetPoint. Status = "Circulating".
If a flow meter sensor is present: If we operate for systemTime and flowPulsesFilteredPerSec < minFlowPulsesPerSec then trigger error state. This can happen at the start of the mode or later due to speed change or other causes.

##### Manual cooling mode (manual_cool)
In addition to the logic in "manual_circ", run the cooling pump at coolingSpeedSetPoint.
Status = "Cooling".

##### Manual heating mode (manual_heat)
In addition to the logic in "manual_circ", run the heating element at heatingSpeedSetPoint.
Status = "Heating".

##### Temperature mode (temperature)

###### Two buckets method

Set new defines:
COOLING_HYSTERESIS_START 1.0f // deg C
COOLING_HYSTERESIS_STOP 0.0f // deg C
HEATING_HYSTERESIS_STOP -1.0f // deg C
HEATING_HYSTERESIS_START -2.0f // deg C
these should be defined such that there is a gap between heating and cooling. Running both at the same time is wasteful.

* Run circulation pump at circSpeedSetPoint
* If outTemperature > temperatureSetPoint + COOLING_HYSTERESIS_START, run cooling pump at coolingSpeedSetPoint.
* Else if outTemperature < temperatureSetPoint + HEATING_HYSTERESIS_START, run heating element at heatingSpeedSetPoint.
* Else keep circulating.
* If heating element in running and outTemperature > temperatureSetPoint + HEATING_HYSTERESIS_STOP, stop heating element
* If cooling pump is running and outTemperature < temperatureSetPoint + COOLING_HYSTERESIS_STOP, stop cooling pump.

Update Status according to operation: "Circulating", "Heating", "Cooling"

###### Variable speed method

If METHOD_VARIABLE_CIRCULATION_SPEED is defined.
In addition to the "two buckets method" logic, except for the part of running circulation at a fixed speed.

When a flow sensor is present, do:

* Start at max speed (255)
* Every "systemTime" interval, if returnTemperature < temperatureSetPoint and flowPulsesFilteredPerSec > minFlowPulsesPerSec, reduce speed by a step of 10 (const, VARIABLE_SPEED_STEP). if returnTemperature > temperatureSetPoint, increase speed by a step of 10 (if we are not at max).
* Every time we sample sensors, if flowPulsesFilteredPerSec < minFlowPulsesPerSec, increase speed by a step of 10 (if we are not at max)

When a flow sensor is not present, assume circSpeedSetPoint has been set as the lower speed the pump will run properly at and do:

* Start at max speed (255)
* Every "systemTime" interval, if returnTemperature < temperatureSetPoint and circSpeed > circSpeedSetPoint, reduce speed by a step of 10 (const, VARIABLE_SPEED_STEP) up to min. if returnTemperature > temperatureSetPoint, increase speed by a step of 10 (up to max).

##### Flow Calibration mode (flow_calibration)

Only valid if a flow meter sensor is present.
When started, save flowPulsesFiltered. Operate circulation pump at circSpeedSetPoint. When mode is stopped, populate calibrationFlowPulses. Set calibrationFlowPulses to the total amount of change to flowPulsesFiltered. 
Assuming the user measured calibrationVolume amount of water during the operation, the system now has valid flow calibration info.
Start with "Calibrating" status, finish with "Calibrated".
If no pulses or flowPulsesFilteredPerSec < minFlowPulsesPerSec at stop time (calibration end), set an error state.
If after systemTime of running, flowPulsesFilteredPerSec < minFlowPulsesPerSec then trigger error state.

##### Flow test mode (flow_test)

Only valid if a flow meter sensor is present.
Tests speed vs flow of the circulation pump.
Run cycles of testing in a loop until mode is changed to something else.
Start each cycle with the circulation pump at max speed. Wait for flowPulsesFilteredPerSec > minFlowPulsesPerSec.
First step wait for FLOW_TEST_FIRST_STEP_INTERVAL_RATIO * systemTime seconds then drop speed by FLOW_TEST_STEP.
Then every (FLOW_TEST_STEP_INTERVAL_RATIO x systemTime) seconds, drop speed by FLOW_TEST_STEP points. If flowPulsesFilteredPerSec < minFlowPulsesPerSec, restart cycle.

### Web Interface

Operate a web site for HMI purposes. Assume device is on an internal LAN. Implement no authentication or security for the web interface.

Separate css from js from html. Store them as files in the ESP32 filesystem.

Store js libraries as static files and use relative urls to the local site so that the site will run correctly even if the internet is not available.

Reference/tutorial for Alpine and Chart: https://www.raymondcamden.com/2023/03/06/adding-a-chart-to-an-aplinejs-application

The web interface will have two pages; the main page and configuration page.

Support just English language in this version.

#### Temperature units

The firmware and back-end use degree Celsius as the unit of temperature. Allow the user to set the unit of temperature on the client side. The options are Celsius and Fahrenheit. Implement the selection control below the "Hostname" control on the configuration page. Save the setting in local storage.
Every time you show a temperature value, convert value from °C to °F if needed and show the appropriate units. Abstract the conversions.
Same thing on the charts. Mind that some temperature values are deltas (like temperatureDelta) and need appropriate conversion.
For the temperature set point, adjust range as well as value. Convert back to °C if needed before posting to the backend.

#### Main page

Fetch telemetry from the backend. Wait 5 seconds (const) between one request end and a new request start. Each time the user changes modes, for 10 times (const) use a lower interval of 1 second (const) between requests.
Provide link to the configuration page.
Store 10 minutes of telemetry data in memory (history). Don't persist it. Let it reset if changing pages or reloading.
Show metrics depending on what sensors are present. For example, don't show flow or flow derived metrics if there is no flow sensor present.

##### Visual look

* Title
* Link to the configuration page
* Mode section with:
  * key metrics: Status, Mode, Error
  * "Temperature Set Point" slider
  * "Start Temperature Goal Mode" button
  * "Start Circulation/Cooling/Heating mode" buttons (depending on matching hardware being present)
  * Start buttons change to a stop button when that mode is currently active.
* Status section with various metrics that are left and are present. Add meaningful icon to each telemetry value label
* History section with charts of key telemetry history with legend
* Calibration section (if flow meter is present)
  * "Start Flow Sensor Calibration: button and calibrationVolume edit field
  * "Start Flow Test Mode" button which changes to a stop button when active.

#### Configuration page

Show all configuration variables and their values except for passwords.
Allow to change (show values in various input elements).

Show configuration row for temperature units.

##### Visual look

* Title
* Link to the main page
* Pairs of label and input elements, one for each setting. Match correct input control for each type (i.e. edit, checkbox, password, slider, etc)
* Save button

On save, send data to backend using POST or fetch and get a response. Show a notification about the result of the save.

#### Backend

Simple backend using ESPAsyncWebServer and JSON to provide the client side with the state and to modify settings when asked by the user.

### MQTT Interface

If a setting called "mqtt" is set to true, connect to mqtt servers using server hostname, username and password.
Reconnect if disconnected.

Register for a "telemetry updated" and "configuration updated" events with the controller. Once that happens publish state as ``${mqttRootTopic}/state``
Bunch all state telemetry and configuration in a json object. Provide all variables except ones relating to mqtt (mqtt*).

Also publish telemetry and configuration on MQTT connect or re-connect.

Subscribe to ``${mqttRootTopic}/+/set`` where + is any configuration property name, update controller with new configuration values if published by somebody. Before passing to the controller, filter out mqtt* configuration values as those are to only be set via the web interface.
Use State's Configuration Variable class to parse and serialize values to strings for MQTT.

#### HA MQTT Discovery

If a setting called "mqttHADiscovery" is set to true, publish needed values to declare our data automatically.
https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery

Use "device discovery" topic: 
``${mqttHADiscoveryTopic}/device/${hostname}/config``

Configure all mqtt supported telemetry and configuration variables. Use ranges, options list, units, etc when possible.

## Future directions

In the basic operation method, the outgoing water might be too cold. Do more testing.

Use a second pump to pass or move cold water from the cold tank to the main tank if the water in the main tank is too hot.

Another option could be two tanks and two pumps then mixing the two streams to achieve a particular temperature.

Another option is to somehow control how much the ice packs are in contact with the water.

If we can control where the returning water goes, then we can send it to either the cold tank or the main tank. Assuming overflow from the cold tank goes to the main tank, then we can lower the temperature of the water in the main tank this way.

Add heating safety mechanisms, like max continuous on time limit, etc.

Use hot water container and cycle hot water for heating similar to how it is done for cooling. Mind that simple aquarium pumps are typically good up to 60 deg C.

## Hardware setups

### Full

3 output drivers for 2 pumps and 1 heating element
3 temperature sensors
1 flow sensor
3 leds
1 button
3 INA226 or 1 INA3221

### Minimal

2 output drivers for 2 pumps
1 temperature sensor
1 led (built-in)
1 button (built-in)
