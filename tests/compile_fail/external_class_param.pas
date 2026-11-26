{* EXPECT: the C function 'F' cannot have the parameter 'k' of type "TK" *}
program T; type TK = class end; procedure F(k: TK); external; begin end.
