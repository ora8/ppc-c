{* EXPECT: its parameter 'k' is a "TK": only numbers, Boolean, Char, enumerations, records of those and String parameters *}
library T; type TK = class end; procedure F(k: TK); begin end; exports F; begin end.
