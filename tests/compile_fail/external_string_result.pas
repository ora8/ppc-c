{* EXPECT: the C function 'F' cannot return a "String" *}
program T; function F: String; external; begin end.
