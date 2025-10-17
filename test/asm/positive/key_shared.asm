DEFINE shared.define_macro 12345
A = shared.define_macro
shared.define_macro

%MACRO shared.define_macro
D&A
A = shared.define_macro
%END

LABEL shared.label_macro
A = shared.label_macro
shared.label_macro

%MACRO shared.label_macro
D&A
A = shared.label_macro
%END
