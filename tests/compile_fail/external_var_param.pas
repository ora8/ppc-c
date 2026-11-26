{* EXPECT: the C function 'F' cannot have the 'var' parameter 'x' *}
program T; procedure F(var x: Integer); external; begin end.
