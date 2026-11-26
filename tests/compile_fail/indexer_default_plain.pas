{* EXPECT: only a property with an index ('property Item[i: Integer]: T ...') can be 'default' *}
program T; type TA = class X: Integer; property P: Integer read X; default; end; begin end.
