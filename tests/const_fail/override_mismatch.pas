{ EXPECT: overriding method "Get" must be declared 'const' like the method it overrides }
program T; type TA = class const function Get: Integer; virtual; end; TB = class(TA) function Get: Integer; override; end; const function TA.Get: Integer; begin Result := 1; end; function TB.Get: Integer; begin Result := 2; end;

begin end.
