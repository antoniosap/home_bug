# home_bug

display: 8 characters, 7 segment + 8 leds
keyboard 1: 8 push buttons

keyboard 2: 3x4 capacitive buttons
CPU: D1 mini ESP 8266 MOD compatibile WEMOS

## schematic

keyboard 1 / display: TM1638
pin 1: 3V3
pin 2: gnd
pin 3: STB - D3
pin 4: CLK - D4
pin 5: DIO - D8

keyboard 2: MPR 121
pin 1: 3V3
pin 2: IRQ - n/c
pin 3: SCL - D1
pin 4: SDA - D2
pin 5: gnd

alimentazione USB

## input / output functions

FUNCTIONS LEDS:
LED 1->CALC             |
LED 2->NETWORK          |
LED 3                   |
LED 4                   |
LED 5                   |
LED 6                   |
LED 7                   |
LED 8                   |

FUNCTIONS KEYS:
MENU BASE               | RPN CALC MODE          |
________________________|________________________|
S-KEY 1->CALC           |
S-KEY 2->NETWORK        |
S-KEY 3                 | ADD +
S-KEY 4                 | SUB -
S-KEY 5                 | MUL *
S-KEY 6                 | DIV /
S-KEY 7                 |
S-KEY 8                 | ENTER =

7 SEGMENT DISPLAY
12345678

KEYPAD MAPPINGS:
 K03->7          K02->8           K11->9
 K02->4          K06->5           K10->6
 K01->1          K05->2           K09->3
 K00->0          K04->.           K08->del
