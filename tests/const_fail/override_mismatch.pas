{* EXPECT: overriding method "Get" must be declared 'const' like the method it overrides *}
program T; type TA = class function Get: Integer; const; virtual; end; TB = class(TA) function Get: Integer; override; end; function TA.Get: Integer; const; begin Result := 1; end; function TB.Get: Integer; begin Result := 2; end;

begin end.
