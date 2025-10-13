DEFINE file.define 0x4644
LABEL file.label
A = file.define
A = file.label

%MACRO macro2
D&A
%END

%MACRO macro
DEFINE macro.define 0x4D44
LABEL macro.label
D&A
A = file.define
A = file.label
A = macro.define
A = macro.label
macro2
%END

macro
