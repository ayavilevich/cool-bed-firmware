# Cool Bed project

## Introduction

This is firmware for a bed cooling project. The firmware will control a hardware device that pumps water from a tank through a special mattress with tubes.
The goal of the device is to cool the person sleeping on the mattress.

### Hardware

* A water tank
* Cold or cool water stored in the tank (can also contain ice or an ice pack)
* A submerged water pump (like one used in an aquarium)
* A water flow sensor (that sends digital pulses out) which is installed at the water return point to the tank
* An outgoing water temperature sensor (Dallas, One Wire based)
* A returning water temperature sensor (Dallas, One Wire based)
* An optional cooling water temperature sensor (Dallas, One Wire based)
* A custom PCB for the project
* An ESP32 micro-controller to run the device
* A motor driver, to control speed of the pump (TB6612 or a generic PWM one like a DRV8871)
* A current and voltage measuring IC (INA226)
* A red error LED
* A green indicator LED
* A built-in ESP32 LED (IO2)
* A built-in ESP32 button (IO0)
* (not relevant for firmware) a buck converter to convert pump DC voltage to 5V for electronics

The device is typically powered by a 9V to 12V PSU. The PSU is chosen to match the max voltage of the pump.

Use built-in LED as indicator of Wi-Fi connectivity to an AP. Blink LED slowly when not connected or connecting. On when connected.
Use green LED as indicator of pump use. Solid when the pump is running.
Use red LED for error state as described below.

For INA226, use INA226_AVERAGE_128, INA226_CONV_TIME_204, resistor = 0.1 Ohm and gain range up to 1.3A. All consts in the relevant source code file.

#### Pins

To be defined in the relevant env in platformio.ini

	-D FLOW_SENSOR_PIN=15
	-D BUTTON_PIN=0
	-D DALLAS_SENSOR_RETURNING_PIN=5
	-D DALLAS_SENSOR_OUTGOING_PIN=18
	-D DALLAS_SENSOR_COOLING_PIN=19
	-D MOTOR_DRIVER_PWM_PIN=27
	-D MOTOR_DRIVER_TB6612_AIN1_PIN=12
	-D MOTOR_DRIVER_TB6612_AIN2_PIN=13
	-D MOTOR_DRIVER_TB6612_STBY_PIN=14
	-D RED_LED_PIN=26
	-D GREEN_LED_PIN=25
	-D BUILTIN_LED_PIN=2

MOTOR_DRIVER_TB6612_* defines are to be present if TB6612 is to be used. Otherwise MOTOR_DRIVER_PWM_PIN is used for a generic PWM motor driver.

Default ESP32 I2C pins 21 and 22.

Button on IO0 can't be used at the moment of the reset but can be used once the device is booting. If button is held for 5 seconds (const) after boot, reset Wi-Fi credentials via WiFiManager and let it load the WiFiManager's UI for the user to select Wi-Fi once again.

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

Create a templated class to facilitate metrics/variables. One instance of the class will be created for each variable. The class will keep the value, implement serializing and parsing to String, serializing and parsing to json value, min/max values, units, description and perform other common functionality that all configuration properties have.

Create a templated class to facilitate configuration variables (inherits from the "metric" class above). One instance of the class will be created for each configuration variable. The class will allow loading and saving to "Preferences" and perform other common functionality that all configuration variables have.
Based on template type, decide Preference type, etc. Use a second shorter key for Preferences that is no more than 16 chars.

Here are the variables of the state that are set by the user and we need to persist (configuration):

| Name               | type   | default | meaning                                             | values                                | web interface     | mqtt interface |
| :----              | :----  | :----   | :----                                               | :----                                 | :----             | :----          |
| hostname           | string | cool-bed| hostname (also id) of the device                    | [a-zA-Z0-9_-]                         | input.text        | Not available  |
| mqtt               | bool   | false   | whether to enable MQTT                              | true/false                            | input.checkbox    | Not available  |
| mqttServer         | string |         | hostname of the mqtt server                         | any string                            | input.text        | Not available  |
| mqttPort           | int    | 1883    | port of the mqtt server (not TLS supported)         | valid port number                     | input.number      | Not available  |
| mqttUsername       | string |         | username for the mqtt server                        | any string                            | input.text        | Not available  |
| mqttPassword       | string |         | password for the mqtt server                        | any string                            | input.password    | Not available  |
| mqttRootTopic      | string | cool-bed| where to publish and subscribe in the hierarchy     | any string                            | input.text        | Not available  |
| mqttHADiscovery    | bool   | true    | whether to enable HA MQTT discovery                 | true/false                            | input.checkbox    | Not available  |
| mqttHADiscoveryTopic|string | homeassistant | match HA configuration for discovery          | any string                            | input.text        | Not available  |
| mode               | string | stop    | how to operate, enum                                | stop, speed, temperature, calibration | individual buttons| Select         |
| speedSetPoint      | byte   | 255     | at which speed to operate in mode=speed             | [1, 255]                              | slider            | Number         |
| temperatureSetPoint| float  | 26      | temperature set point in mode=temperature           | [10, 40]                              | slider            | Number         |
| calibrationVolume  | uint   | 500     | volume in ml that was used to calibrate flow        | [1, 5000]                             | input.number      | Number         |
| calibrationFlow    | float  | 1       | end flow in liters/min during calibration           | [0, 100]                              | input.number      | Number         |
| calibrationFlowPulses| uint | 500     | filtered flow pulses measured while calibrating flow| [1, 10000]                            | input.number      | Number         |
| systemTime         | uint   | 30      | seconds to respond to a change in input             | [0, 600]                              | input.number      | Number         |
| minFlowPulsesPerSec| uint   | 5       | minimum filtered flow pulses per sec to consider flow | [1, 1000]                           | input.number      | Number         |
| maxCurrent         | uint   | 1000    | mA of pump current after which to trigger error     | [1, 1200]                             | input.number      | Number         |
| minVoltage         | uint   | 4000    | mV of pump voltage below which to trigger error     | [0, 40000]                            | input.number      | Number         |
| outTemperatureCalibrationOffset    | float  | 0       | value to add to out temperature sensor reading    | [-10, 10]               | input.number      | Number         |
| returnTemperatureCalibrationOffset | float  | 0       | value to add to return temperature sensor reading | [-10, 10]               | input.number      | Number         |
| coolingTemperatureCalibrationOffset | float  | 0   | value to add to cooling water temperature sensor reading | [-10, 10]        | input.number      | Number         |

Note: calibrationFlow and calibrationFlowPulses are user-editable config but also can be set by the firmware. This is to allow tuning by the user. Assume most of the time they will be automatically calculated.

Here are the variables of the state that are metrics from the device which we need to collect and report (telemetry)

| Name               | type   | default | meaning                                         | values         | web interface     | mqtt interface |
| :----              | :----  | :----   | :----                                           | :----          | :----             | :----          |
| status             | string |         | what the device is currently doing              | any string     | div/span          | Sensor         |
| error              | bool   | false   | are we in error state right now?                | true/false     | icon              | Binary sensor  |
| pumpSpeed          | byte   | 0       | at which speed the pump is operated right now   | [0, 255]       | div/span          | Sensor         |
| flowPulsesRaw      | uint64 | 0       | flow pulses (absolute, measured by interrupt)   | any integer    | div/span          | Sensor         |
| flowPulsesFiltered | uint64 | 0       | flow pulses (absolute, measured by sampling)    | any integer    | div/span          | Sensor         |
| pumpVoltage        | uint   | 0       | pump voltage in mV                              | any integer    | div/span          | Sensor         |
| pumpCurrent        | int    | 0       | pump current in mA                              | any integer    | div/span          | Sensor         |
| outTemperature     | float  | 20      | outgoing water temperature in C                 | [-5, 40]       | div/span          | Sensor         |
| returnTemperature  | float  | 20      | returning water temperature in C                | [-5, 40]       | div/span          | Sensor         |
| coolingTemperature | float  | 20      | cooling water temperature in C                  | [-5, 40]       | div/span          | Sensor         |

Here are some calculated metrics that are based on other metrics (more telemetry)

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

If there is no saved Wi-Fi network to connect to, use WiFiManager library per its default mode of operation. Use WFM AP SSID = hostname variable value.
Once a network was configured, connect to that network.
Reconnect to the network if the connection is lost.
Register hostname.local with mDNS.

### Controller

Sample sensors and update telemetry.
Operate pump per specified mode.
Error state is when a red LED is lit, pump is stopped and state reports an error (preferably with cause).
When mode is changed by the user, reset error state (LED, etc).
At any time when pumpCurrent > maxCurrent or pumpVoltage < minVoltage, trigger an error state.
At any time when INA overflows or reports an error, trigger an error state.

It can take 30 seconds for the change in pump to affect the returning temperature and/or flow. Plan accordingly and use systemTime in the logic.

#### Sensors

Sample sensors every second.
See difference in pulses from last sample. Calculate derived telemetry based on difference and calibration settings.
If error reading from outgoing or returning temperature sensor, or if returned value is invalid, retry up to 3 (const) times and trigger an error state.
Cooling water temperature sensor is optional. Retry reads similarly, but do not trigger error state if this sensor is missing or invalid.
Valid sensor temperature range is -5 to 40 degrees celsius (consts).

##### Flow sensor

flowPulsesRaw is the count of RISING interrupts on the pin. Could have noise on some systems.
flowPulsesFiltered is more complex. Implement in a separate class. Maintain a uint16 pin history state, current bool state and up and down counters. Setup a timer to fire every 50 microseconds (constant). In the timer interrupt, read the level of the pin with gpio_get_level. Push the pin level in the history as a bit (FIFO). This way the history keeps the latest 16 levels of the pin. When history changes to UINT16_MAX or 0 derive new state. If new state different from current state then inc up or down counter and update current state. Init history per gpio_get_level value at init time.

#### Operation modes

##### Stop mode
Pump is stopped. If a user moved us to this mode then a status = "stopped".
If after 10 minutes (const) of being stopped, absolute deltaTemperature value is over 0.2 deg C (const) then set status to a warning text:
"Warning: temperature sensors might not be calibrated"

##### Speed mode
Operate pump at speedSetPoint. Status = "speed".
If we operate for systemTime and flowPulsesFilteredPerSec < minFlowPulsesPerSec then trigger error state. This can happen at the start of the mode or later due to speed change or other causes.

##### Temperature mode
Maintain current pump speed in currentSpeed variable.

* Start at max speed (255)
* Every "systemTime" interval, if returnTemperature < temperatureSetPoint and flowPulsesFilteredPerSec > minFlowPulsesPerSec, reduce speed by a step of 10 (const). if returnTemperature > temperatureSetPoint, increase speed by a step of 10 (if we are not at max).
* Every time we sample sensors, if flowPulsesFilteredPerSec < minFlowPulsesPerSec, increase speed by a step of 10 (if we are not at max)

Open questions:
* Once we reduce speed to the point where the flow is too low, can we recover by increasing the speed or do we need to go much higher and go back down from there? Test once "speed" mode is implemented.

##### Calibration mode

When started, save flowPulsesFiltered. Operate pump at speedSetPoint. When mode is stopped, populate calibrationFlow and calibrationFlowPulses. Set calibrationFlow to the last flow value before the pump is stopped. Set calibrationFlowPulses to the total amount of change to flowPulsesFiltered. Assuming the user measured calibrationVolume amount of water during the operation, the system now has valid calibration info.
Start with "calibrating" status, finish with "calibrated".
If no pulses or flowPulsesFilteredPerSec < minFlowPulsesPerSec at stop time (calibration end), set an error state.
If after systemTime of running, flowPulsesFilteredPerSec < minFlowPulsesPerSec then trigger error state.

##### Flow test mode

Run cycles of testing until mode is changed to something else.
Start each cycle with the pump at max speed. Wait for flowPulsesFilteredPerSec > minFlowPulsesPerSec.
Then every (ratio x systemTime) seconds, drop speed by 10 points. If flowPulsesFilteredPerSec < minFlowPulsesPerSec, restart cycle.

### Web Interface

Operate a web site for HMI purposes. Assume device is on an internal LAN. Implement no authentication or security for the web interface.

Separate css from js from html. Store them as files in the ESP32 filesystem.

Store js libraries as static files and use relative urls to the local site so that the site will run correctly even if the internet is not available.

Reference/tutorial for Alpine and Chart: https://www.raymondcamden.com/2023/03/06/adding-a-chart-to-an-aplinejs-application

The web interface will have two pages; the main page and configuration page. "Pages" don't have to be separate pageviews. Can all be in the same SPA if that is the best practice for Alpine.js . Also, settings can be expandable panel in the main page.

Support just English language in this version.

#### Main page

Fetch telemetry from the backend. Wait 5 seconds (const) between one request end and a new request start. Each time the user changes modes, for 10 times (const) use a lower interval of 1 second (const) between requests.
Provide link to the configuration page.
Store 10 minutes of telemetry data in memory (history). Don't persist it. Let it reset if changing pages or reloading.

##### Visual look
* Title
* Link to the configuration page
* Stop button (if not stopped)
* Temperature mode: Start button and temperature slider
* Speed mode: Start button and speed slider
* Calibrate mode: Start button and calibrationVolume edit field
* Latest telemetry values (including state, current mode, etc). Add meaningful icon to each telemetry value label
* Chart of all the available telemetry history with legend

#### Configuration page

Show all configuration variables and their values except for passwords.
Allow to change (show values in various input elements).

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

In the basic operation mode, the outgoing water might be too cold. Do more testing. It might be necessary to have two tanks, one for the water that is circulating in the mattress and another one for cold water. Use a second pump to pass or move cold water from the cold tank to the main tank if the water in the main tank is too hot.

Another option could be two tanks and two pumps then mixing the two streams to achieve a particular temperature.

Another option is to somehow control how much the ice packs are in contact with the water.

If we can control where the returning water goes, then we can send it to either the cold tank or the main tank. Assuming overflow from the cold tank goes to the main tank, then we can lower the temperature of the water in the main tank this way.

TBD, Thermostatic mixing valve
https://he.aliexpress.com/item/1005005680230789.html
DC 3.7V 6V 12V 0420 Micro Solenoid Valve 2-position 3-way Water Valve Electric Control Water Air Valve Electromagnetic Valve
