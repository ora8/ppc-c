{ EXPECT: 'p' is nil here (dereference of nil) }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end;
procedure P(p: nilable TFoo); begin if p = nil then WriteLn(p.V); end;
begin end.
