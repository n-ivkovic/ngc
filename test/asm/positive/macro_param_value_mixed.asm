%MACRO macro.2 param1 param2
A = param1
A = param2
%END

DEFINE key.define 0x4B44
LABEL key.label

macro.2 12345 key.define
macro.2 12345 key.label
macro.2 key.define 12345
macro.2 key.label 12345
macro.2 12345 key.define
macro.2 12345 key.label
