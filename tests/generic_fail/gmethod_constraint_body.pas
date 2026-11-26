{* EXPECT: operator needs the constraint 'T: Numeric' (in Sum<T>) *}
program T;
type TA = class function Sum<T>(a, b: T): T; end; function TA.Sum<T>(a, b: T): T; begin Result := a + b; end;
begin end.
