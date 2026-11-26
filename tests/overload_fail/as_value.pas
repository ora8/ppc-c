{* EXPECT: "P" is overloaded and cannot be used as a value *}
program T;
procedure P(i: Integer); overload; begin end; procedure P(s: String); overload; begin end; val f: procedure(i: Integer) := P;
begin end.
