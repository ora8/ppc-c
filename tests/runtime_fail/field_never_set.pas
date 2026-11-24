{ EXPECT: Runtime error 210: nil pointer dereference at runtime_fail/field_never_set.pas(6,22) }
{ FLAGS: --no-nil-check }
program T;
type TN = class V: Integer; Child: TN; end;
vao n: TN := TN.Create;
begin WriteLn(n.Child.V); n.Free; end.
