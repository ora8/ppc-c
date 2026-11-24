{ EXPECT: 'V' is not available in a class method (a class method has no Self) }
program T;
type TK = class V: Integer; class function F: Integer; end; class function TK.F: Integer; begin Result := V; end;
begin end.
