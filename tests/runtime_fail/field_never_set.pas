{ EXPECT: Runtime error 210: nil pointer dereference at runtime_fail/field_never_set.pas(6,38) }
{ FLAGS: --no-nil-check }
program T;
type TN = class V: Integer; Child: TN; end;
var n: TN;
begin n := TN.Create; WriteLn(n.Child.V); n.Free; end.
