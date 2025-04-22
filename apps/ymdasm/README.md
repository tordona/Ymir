# ymdasm
Ymir disassembly tool, supporting SH-2, Motorola 68000, Saturn SCU DSP and SCSP DSP ISAs.

## Usage

Disassemble SH-2 code:
```sh
ymdasm sh2 3233 8BF8 000B
```
```
3233  cmp/ge    r3, r2
8BF8  bf        #0xFFFFFFF4
000B  > rts
```

Display addresses and change origin:
```sh
ymdasm sh2 -a -o 6000180 3233 8BF8 000B
```
```
6000180  3233  cmp/ge    r3, r2
6000182  8BF8  bf        #0x6000174
6000184  000B  > rts
```

Force delay slot for first instruction:
```sh
ymdasm sh2 -d 000B 000B
```
```
000B  > (illegal)
000B  rts
```
