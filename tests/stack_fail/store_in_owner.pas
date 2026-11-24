{ EXPECT: 's' is a stack object and stays in its block: it cannot be moved into 'o' (borrow it with 'var', or store a copy: 's.Clone') }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Mach: TFoo; begin Result := TFoo.Create(1); end;
procedure P; val s := TFoo.Create(1); vao o: TFoo := s; begin end;
begin P; end.
