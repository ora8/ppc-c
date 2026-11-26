{* EXPECT: "P(Integer)" is overloaded twice with the same parameters *}
program T;
procedure P(i: Integer); overload; begin end; procedure P(j: Integer); overload; begin end;
begin end.
