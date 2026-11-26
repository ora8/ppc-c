{* EXPECT: a method cannot be external *}
program T; type TK = class procedure P; end; procedure TK.P; external; begin end.
