{ EXPECT: borrowed reference 'k' is used after its owner 'a' was resized by SetLength at line 4 }
program T;
type TK = class N: Integer; end;
procedure P; vao a: array of optional TK := default; begin SetLength(a, 1); a[0] := TK.Create; if a[0] is some k then begin SetLength(a, 0); WriteLn(k.N); end; end;
begin P; end.
