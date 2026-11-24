{ EXPECT: reference to local variable 's' escapes into 'G' (it would dangle after the routine returns) }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Mach: TFoo; begin Result := TFoo.Create(1); end;
var G: optional TFoo := none;
procedure P; val s := TFoo.Create(1); begin G := s; end;
begin P; end.
