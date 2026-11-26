{* EXPECT: use of 'a' after it may have been freed *}
program T; type TFoo = class V: Integer; end;
vao a: TFoo := TFoo.Create; val c: Boolean := default;
begin c := True;  if c then a.Free; WriteLn(a.V); end.
