{ EXPECT: the method "Id" of "TA" is private }
program T;
type TA = class private function Id<T>(x: T): T; end; function TA.Id<T>(x: T): T; begin Result := x; end; procedure P; vao a := TA.Create; begin WriteLn(a.Id(3)); end;
begin end.
