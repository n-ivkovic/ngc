macro1
macro2 val1 val2
macro2 val2 val1
macro2 0x7FFF val2
macro2 val1 0x7FFF

DEFINE val1 1

%MACRO macro1
DEFINE val3.1 3

D&A
A = val1
A = val2
A = val3.1
A = val4.1

DEFINE val4.1 4
%END

%MACRO macro2 param1 param2
DEFINE val3.2 3

A = param1
A = val4.2
A = val3.2
A = val2
A = val1
A = param2
D&A

DEFINE val4.2 4
%END

DEFINE val2 2

macro1
macro2 val1 val2
macro2 val2 val1
macro2 0x7FFF val2
macro2 val1 0x7FFF
