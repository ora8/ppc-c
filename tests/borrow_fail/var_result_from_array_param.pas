{ EXPECT: borrowed reference 'r' is used after its owner 'a' was resized by SetLength }
program T;
type TK = class end;
function Get(var a: array of optional TK): var optional TK; begin Result := a[0]; end;
procedure Use2(x: TK); begin end;
procedure P(var a: array of optional TK); begin if Get(a) is some r then begin SetLength(a, 0); Use2(r); end; end;
begin end.
