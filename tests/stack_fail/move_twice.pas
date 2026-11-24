{ EXPECT: use of moved stack object 's' ('s' was moved to 'a' at line 4) }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; val s := TFoo.Create(1); val a := s; val b := s; begin end;
begin P; end.
