{* EXPECT: "TK" does not satisfy 'T: Comparable' (required by Big<T>) *}
program T;
type TA = class function Big<T: Comparable>(x: T): Boolean; end; function TA.Big<T>(x: T): Boolean; begin Result := True; end; type TK = class end; procedure P; vao a := TA.Create; vao k := TK.Create; begin WriteLn(a.Big(k)); end;
begin end.
