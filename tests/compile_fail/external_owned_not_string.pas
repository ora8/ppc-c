{* EXPECT: 'owned' is for a C function whose String result the caller frees *}
program T;
function F(x: Integer): Integer; external; owned;
begin end.
