{ EXPECT: 's' is a stack object and cannot be copied or moved: borrow it ('var t: TFoo := s') or copy it ('vao t := s.Clone') }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Mach: TFoo; begin Result := TFoo.Create(1); end;
procedure P; val s := TFoo.Create(1); val t := s; begin end;
begin P; end.
