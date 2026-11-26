{* EXPECT: use of moved stack object 's' ('s' was moved to the heap (parameter 'x' of 'Extract') at line 5) *}
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure Extract(vao x: TFoo); begin end;
procedure P; val s := TFoo.Create(1); begin Extract(s); s.V := 2; end;
begin P; end.
