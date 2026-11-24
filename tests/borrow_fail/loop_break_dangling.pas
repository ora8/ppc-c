{ EXPECT: borrowed reference 'r' is used after its owner 'k' freed its object at the end of its block (line 4) }
program T;
type TK = class N: Integer; constructor Create(AN: Integer); end; constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
procedure P; var r: optional TK := none; begin loop vao k := TK.Create(1); begin r := k; Break; end; if r is some x then WriteLn(x.N); end;
begin P; end.
