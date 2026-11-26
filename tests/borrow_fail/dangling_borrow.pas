{* EXPECT: borrowed reference 'r' is used after its owner 'a' was freed *}
program T; type TFoo = class V: Integer; end;
vao a: TFoo := TFoo.Create; var r: TFoo := a;
begin   a.Free; WriteLn(r.V); end.
