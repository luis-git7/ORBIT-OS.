ORBIT OS 2.0 – ESP32-C3 Mini Handheld

Full tutorial on my TT: www.tiktok.com/@gpiox46

ORBIT OS is a DIY handheld gaming console based on the ESP32-C3. It features an SH1106 OLED display, five control buttons, a buzzer and eight built-in games.

Features:
ESP32-C3 Mini microcontroller.
1.3 inch SH1106 128×64 OLED display.
8 built-in games.
Five physical control buttons.
Buzzer sound effects.
3000 mAh Li-Ion battery.
TP4056 battery charging module.

4056 charging current is configured using its PROG resistor.
The resistor value must be selected according to the battery manufacturer's maximum permitted charging current and the TP4056 module's specifications.
Different batteries may require different charging currents and therefore different PROG resistor values.
Do not assume that the resistor supplied with the module is suitable for every battery.
Step-up voltage converter.

Wiring Diagram:
<img width="1024" height="1536" alt="image" src="https://github.com/user-attachments/assets/56a003e1-5b96-4935-9e56-561c6b591aa4" />

Safety Notes:
Always check the battery manufacturer's charging specifications.
Verify the TP4056 PROG resistor before charging the battery.
Set the step-up converter to 5V before connecting the ESP32.
Never connect 5V directly to an ESP32 GPIO.
Check that the OLED's SDA and SCL pull-ups do not pull the I2C lines to 5V. ESP32-C3 GPIOs are not 5V tolerant.
The battery, charger, converter and ESP32 must have a common electrical ground.
A standard TP4056 module does not necessarily provide proper load sharing. Avoid operating the handheld while charging unless the power circuit supports simultaneous charging and operation.
Use a TP4056 module with suitable battery protection, or provide separate protection against over-discharge.

Software:
The firmware is written for the Arduino IDE.
Required libraries:
Adafruit GFX Library
Adafruit SH110X
Preferences (included with the ESP32 Arduino core)
Open the .ino file in Arduino IDE, select your ESP32-C3 board and upload the firmware.

Project:
ORBIT OS is a DIY maker project. The hardware and software can be modified and improved.

Have fun building!
<img width="1536" height="2048" alt="image" src="https://github.com/user-attachments/assets/0f3b440a-5b1e-40f9-ad3c-e36b484dca99" />
<img width="1536" height="2048" alt="image" src="https://github.com/user-attachments/assets/851bc020-34df-4464-9c67-971c83947cf8" />
<img width="1536" height="2048" alt="image" src="https://github.com/user-attachments/assets/cc81bca1-26bd-4383-acd8-a27ac81f34cc" />
<img width="1152" height="2048" alt="image" src="https://github.com/user-attachments/assets/fa08bbfc-646f-4be7-86b3-c9e663a31472" />
<img width="750" height="1334" alt="image" src="https://github.com/user-attachments/assets/d99fb0b2-e893-4556-ab60-abf19dba3e43" />
<img width="750" height="1334" alt="image" src="https://github.com/user-attachments/assets/653ceb9d-74db-4feb-b361-65bb4621d073" />
