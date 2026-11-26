{* EXPECT: write the modifier after the type: 'T const' *}
program T;
type TK = class end; procedure P(x: const TK); begin end;
begin end.
