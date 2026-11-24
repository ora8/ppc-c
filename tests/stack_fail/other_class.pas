{ EXPECT: stack object 's' is declared as "TFoo" but created as "TBar": it is stored in place, so both must be the same class }
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end; function Make: TFoo; begin Result := TFoo.Create(1); end;
procedure P; val s: TFoo := TBar.Create(1); begin end;
begin P; end.
