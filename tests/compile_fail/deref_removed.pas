{* EXPECT: pointer dereference '^' is not supported *}
program T;
type TFoo = class V: Integer; end;
val f: TFoo;
begin f.Create; WriteLn(f^.V); end.
