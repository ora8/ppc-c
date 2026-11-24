{ EXPECT: optional case arms are 'some x:' and 'none:' }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure P(p: optional Integer); begin case p of 1: WriteLn(1); end; end;
begin end.
