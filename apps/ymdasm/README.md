# ymdasm
Ymir disassembly tool, supporting SH-2, Motorola 68000, Saturn SCU DSP and SCSP DSP ISAs.



## Usage

The command syntax is:

```sh
ymdasm [GLOBAL OPTIONS] \
    <isa 1> {[ISA 1 OPTIONS]|<program bytes>}... \
   [<isa 2> {[ISA 2 OPTIONS]|<program bytes>}... [...]]
```

`ymdasm` supports the following instruction set architectures used by the Sega Saturn:
- `sh2`: Hitachi SH-2 (big-endian)
- `m68k`: Motorola 68000 (big-endian)
- `scudsp`: SCU DSP (big-endian)
- `scspdsp`: SCSP DSP (big-endian)

The instruction set architecture argument is case-insensitive.

Passing an ISA argument initializes a new disassembly context with that instruction set architecture.
`ymdasm` does not assume a default ISA; you must always specify one of the supported ISA arguments to disassemble code.

The program parses arguments sequentially. Certain options apply globally and must be passed before specifying an ISA.
Options that apply to ISAs must be passed after the ISA argument.

`ymdasm` accepts short and long options, similar to GNU's `getopt` and `getopt_long` functions. Short options can be
combined into one argument. If they take arguments, they will be parsed in the order specified in the argument. For
example, `-abc X Y Z` is equivalent to `-a X -b Y -c Z`.

You may use an equals sign to set options, such as `-a=X` or `--foo=bar`.

Append `-` to a flag option to disable it. For instance, `-a` enables option `a`, and `-a-` disables it. To disable
flags in a combined series of short options, append `-` after the flags you wish to disable: `-ab-c-de` enables
options `a`, `d` and `e`, and disables options `b` and `c`.



### Global options

`ymdasm` accepts the following global options:

|Short|Long|Arguments|Description|
|:--|:--|:--|
|`-c`|`--color`|flag|Enables colored output using ANSI 16-color escape codes. Ignored when outputting to files.|
|`-C`|`--high-color`|flag|Enables colored output using ANSI 24-bit color escape codes. Ignored when outputting to files.|



### Common disassembler options

These options can be used with all ISAs:

|Short|Long|Arguments|Description|
|:--|:--|:--|
|`-i`|`--input-file`|string|Disassembles code from the specified file. If omitted, disassembles opcodes from the command line.|
|`-s`|`--input-offset`|unsigned integer|Specifies the offset into the input file from which to start disassembling code. (default: 0)|
|`-l`|`--input-length`|unsigned integer|Specifies the length (in bytes) of the code to read from the input file. (default: maximum possible)|
|`-f`|`--output-file`|string|Outputs disassembled code to the specified file. If omitted, prints to stdout.|
|`-o`|`--org`, `--origin`|unsigned integer|Specifies the origin (base) address of the disassembled code. (default: 0)|
|`-a`|`--print-address`|flag|Prints addresses in the disassembly listing. (default: enabled)|
|`-c`|`--print-opcodes`|flag|Prints opcodes in the disassembly listing. (default: enabled)|
|`-E`|`--big-endian`|flag|Force big-endian parsing. Mutually exclusive with `-e`. (default: depends on ISA)|
|`-e`|`--little-endian`|flag|Force little-endian parsing. Mutually exclusive with `-E`. (default: depends on ISA)|

Multiple input files can be specified and interweaved with opcodes directly specified in the command line. The resulting
disassembly contains the merged contents of all files and opcodes in the specified order. Each input file takes its own
set of parameters and does not reuse previously configured options. For instance:
```sh
ymdasm sh2 -isl 8 4 file1.bin 0009 -i file2.bin ca00
```
will disassemble 4 bytes of SH-2 code from `file1.bin` starting from offset 8, followed by the opcode `0009`, then the
entire contents of `file2.bin`, and finally the opcode `ca00`.

`<program bytes>` are a sequence of hexadecimal values representing the program bytes. Individual space-separated values
are left padded with zeros to an even number of digits, the converted to bytes using the ISA's endianness.

For example, with a big-endian ISA:
- `1 2 3 4` is padded to `01 02 03 04`
- `1 23 456` is padded to `01 23 0456` then converted to `01 23 04 56`
- `12 34 56 78`, `1234 5678`, `12 345678` and `12345678` are all equivalent

With a little-endian ISA:
- `1 23 456` is padded to `01 23 0456` then converted to `01 23 56 04`
- `12 34 56 78`, `3412 7856`, `12 785634` and `78563412` are all equivalent

Partial opcodes are left padded with zeros to full opcodes. For a 16-bit ISA, the opcode `9` is interpreted as `0009`.

`-e` and `-E` lets you override the endianness for subsequent opcodes. `-e 1234 -E 1234` produces `12 34 34 12`.



### Hitachi/Renesas SH-2 (`sh2`)

The [Hitachi/Renesas SH-2](https://en.wikipedia.org/wiki/SuperH) is a big-endian architecture with fixed-length 16-bit
opcodes.

You can find an opcode table [here](https://shared-ptr.com/sh_insns.html).

This disassembler accepts the following options in addition to the common options:

|Short|Long|Arguments|Description|
|:--|:--|:--|
|`-d`|`--delay-slot`|(flag)|Forces the next opcode to be disassembled as if it were in a delay slot. (default: disabled)|

You can pass `-d` as many times as needed. For example, `ymdasm sh2 -d 0009 0009 -d 0009` will cause the first `nop` to
be decoded as if it were in a delay slot, the second `nop` as a normal instruction, and the third `nop` in a delay slot.
This option is invalid if passed in odd bytes. If a file follows the option, the first instruction will be interpreted
as being in a delay slot.



### Motorola 68000 (`m68k`)

The [Motorola 68000 instruction set](https://en.wikipedia.org/wiki/Motorola_68000) is big-endian with variable-length
16-bit opcodes.

You can find an opcode table [here](http://goldencrystal.free.fr/M68kOpcodes-v2.3.pdf).



### SCU DSP (`scudsp`)

The Saturn SCU (System Control Unit) DSP uses 32-bit big-endian [VLIW](https://en.wikipedia.org/wiki/Very_long_instruction_word)
opcodes and is capable of executing up to 6 instructions at once. It is often used to offload 3D geometry calculations.

You can find an opcode table [here](https://github.com/srg320/Saturn_hw/blob/main/SCU/SCU%20DSP.xlsx).



### SCSP DSP (`scspdsp`)

The SCSP (Saturn Custom Sound Processor) DSP uses 64-bit big-endian instructions specialized for audio processing.

The best source of information about this ISA is Neill Corlett's research on the Dreamcast's AICA (the successor of the
SCSP), which has been mirrored [here](https://raw.githubusercontent.com/Senryoku/dreamcast-docs/refs/heads/master/AICA/DOCS/myaica.txt).



### Disassembling multiple architectures simultaneously

Multiple instantiations of ISAs are possible using a single command. For example:

```sh
ymdasm sh2 -ao 6000000 0009   m68k -a 4e71   sh2 -a 000b
```
will result in:
```
; Hitachi SH-2
6000000  0009  nop

; Motorola 68000
0  4E71  nop

; Hitachi SH-2
0  000b  rts
```

Note that you can instantiate the same ISA multiple times and each instantiation resets the options, as seen in the two
separate SH-2 disassembly listings.



## Examples

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

Disassemble Motorola 68000 code:
```sh
ymdasm m68k -a -o 10000 807b f566 e82f
```
```
10000  807b f566 e82f  or.w     ([#0x1e831, pc]), d0
```

Disassemble M68000 and SH-2 code in one go:
```sh
ymdasm m68k -a -o 10000 807b f566 e82f 5c78 4000  sh2 -a -o 6000180 3233 8BF8 000B
```
```
; Motorola 68000
10000  807b f566 e82f  or.w     ([#0x1e831, pc]), d0
10006  5c78 4000       addq.w   #0x6, #0x4000.w

; Hitachi SH-2
6000180  3233  cmp/ge    r3, r2
6000182  8BF8  bf        #0x6000174
6000184  000B  > rts
```
