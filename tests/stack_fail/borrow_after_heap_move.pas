{* EXPECT: borrowed reference 'r' is used after its owner 's' was moved to the heap ('h') at line 4 *}
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; val s := TFoo.Create(1); var r: TFoo := s; vao h: TFoo := s; begin WriteLn(r.V); end;
begin P; end.
