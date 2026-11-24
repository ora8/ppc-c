{ EXPECT: use of possibly moved stack object 's' ('s' was moved to 't' at line 4) }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P(c: Boolean); val s := TFoo.Create(1); begin if c then val t := s; begin end; WriteLn(s.V); end;
begin P(True); end.
