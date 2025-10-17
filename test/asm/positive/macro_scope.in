DEFINE file.define 0xD0
LABEL file.label
A = file.label
A = file.define

macro.0

%MACRO macro.0
DEFINE macro.define 0xD1
LABEL macro.label
A = macro.label
A = macro.define

A = file.label
A = file.define

macro.2 macro.define macro.label
%END

%MACRO macro.2 param.define param.label
DEFINE macro.define 0xD2
LABEL macro.label
A = macro.label
A = macro.define

A = param.label
A = param.define

A = file.label
A = file.define
%END

macro.0
