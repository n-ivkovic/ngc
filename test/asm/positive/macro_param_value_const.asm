%MACRO macro.1 param1
A = param1
%END

%MACRO macro.2 param1 param2
A = param1
A = param2
%END

macro.1 0
macro.2 0 0
macro.1 00
macro.2 00 00
macro.1 000
macro.2 000 000
macro.1 1
macro.2 1 1
macro.1 01
macro.2 01 01
macro.1 001
macro.2 001 001
macro.1 9
macro.2 9 9
macro.1 09
macro.2 09 09
macro.1 009
macro.2 009 009
macro.1 10
macro.2 10 10
macro.1 010
macro.2 010 010
macro.1 0010
macro.2 0010 0010
macro.1 32767
macro.2 32767 32767
macro.1 032767
macro.2 032767 032767
macro.1 0032767
macro.2 0032767 0032767
macro.1 12345
macro.2 12345 12345

macro.1 0x0
macro.2 0x0 0x0
macro.1 0x00
macro.2 0x00 0x00
macro.1 0x000
macro.2 0x000 0x000
macro.1 0x1
macro.2 0x1 0x1
macro.1 0x01
macro.2 0x01 0x01
macro.1 0x001
macro.2 0x001 0x001
macro.1 0xF
macro.2 0xF 0xF
macro.1 0x0F
macro.2 0x0F 0x0F
macro.1 0x00F
macro.2 0x00F 0x00F
macro.1 0x10
macro.2 0x10 0x10
macro.1 0x010
macro.2 0x010 0x010
macro.1 0x0010
macro.2 0x0010 0x0010
macro.1 0x7FFF
macro.2 0x7FFF 0x7FFF
macro.1 0x07FFF
macro.2 0x07FFF 0x07FFF
macro.1 0x007FFF
macro.2 0x007FFF 0x007FFF
macro.1 0X1ABC
macro.2 0X1ABC 0X1ABC
macro.1 0x1abc
macro.2 0x1abc 0x1abc
macro.1 0X1aBc
macro.2 0X1aBc 0X1aBc
macro.1 0x_1ABC
macro.2 0x_1ABC 0x_1ABC
macro.1 0x1A_BC
macro.2 0x1A_BC 0x1A_BC
macro.1 0x1ABC_
macro.2 0x1ABC_ 0x1ABC_
macro.1 0x__1A__BC__
macro.2 0x__1A__BC__ 0x__1A__BC__
macro.1 0x_
macro.2 0x_ 0x_

macro.1 0b0
macro.2 0b0 0b0
macro.1 0b00
macro.2 0b00 0b00
macro.1 0b000
macro.2 0b000 0b000
macro.1 0b1
macro.2 0b1 0b1
macro.1 0b01
macro.2 0b01 0b01
macro.1 0b001
macro.2 0b001 0b001
macro.1 0b10
macro.2 0b10 0b10
macro.1 0b010
macro.2 0b010 0b010
macro.1 0b0010
macro.2 0b0010 0b0010
macro.1 0b0111111111111111
macro.2 0b0111111111111111 0b0111111111111111
macro.1 0b00111111111111111
macro.2 0b00111111111111111 0b00111111111111111
macro.1 0b000111111111111111
macro.2 0b000111111111111111 0b000111111111111111
macro.1 0B11100100
macro.2 0B11100100 0B11100100
macro.1 0b11100100
macro.2 0b11100100 0b11100100
macro.1 0b_11100100
macro.2 0b_11100100 0b_11100100
macro.1 0b1110_0100
macro.2 0b1110_0100 0b1110_0100
macro.1 0b11100100_
macro.2 0b11100100_ 0b11100100_
macro.1 0b__1110__0100__
macro.2 0b__1110__0100__ 0b__1110__0100__
macro.1 0b_
macro.2 0b_ 0b_
