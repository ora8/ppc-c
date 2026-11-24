{ EXPECT: use of possibly moved value 'a' (ownership was transferred away) }
program T;
type TK = class N: Integer; constructor Create(AN: Integer); end; constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
procedure Extract(vao k: TK); begin end;
procedure P; vao a := TK.Create(1); begin loop begin Extract(a); end; end;
begin P; end.
