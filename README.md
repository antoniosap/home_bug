# home_bug

display: 8 characters, 7 segment + 8 leds
keyboard 1: 8 push buttons

keyboard 2: 3x4 capacitive buttons
CPU: D1 mini ESP 8266 MOD compatibile WEMOS

## schematic

keyboard 1 / display: TM1638
pin 1: 3V3
pin 2: gnd
pin 3: STB - D5
pin 4: CLK - D6
pin 5: DIO - D7

keyboard 2: MPR 121
pin 1: 3V3
pin 2: IRQ - n/c
pin 3: SCL - D1
pin 4: SDA - D2
pin 5: gnd

CPU:
attenzione: lasciare libero D3 che serve per il flash programming
attenzione: va disconnesso D3 dal carico se si riprogramma via USB
attenzione: il Wemos D1 mini a il D8 collegato a GND con un 10k
attenzione: il Wemos D1 mini a il D3 / D4 collegato a 3V3 con un 10k

alimentazione USB

## input / output functions

FUNCTIONS LEDS:
LED 0->CALC             |
LED 1->NETWORK          |
LED 2->WALL CLOCK       |
LED 3                   |
LED 4                   |
LED 5                   |
LED 6                   |
LED 7                   |

FUNCTIONS KEYS:
MENU BASE               | RPN CALC MODE          |
________________________|________________________|
S-KEY 0->CALC           |
S-KEY 1->NETWORK        |
S-KEY 2                 | ADD +
S-KEY 3                 | SUB -
S-KEY 4                 | MUL *
S-KEY 5                 | DIV /
S-KEY 6                 |
S-KEY 7                 | ENTER =

7 SEGMENT DISPLAY
12345678

KEYPAD MAPPINGS:
 K03->7          K02->8           K11->9
 K02->4          K06->5           K10->6
 K01->1          K05->2           K09->3
 K00->0          K04->.           K08->del

