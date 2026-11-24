{ EXPECT: borrowed reference 'r' is used after its owner 'a' was freed at line 4 }
program T;
type TK = class V: Integer; constructor Create; class function Neu: TK; end; constructor TK.Create; begin inherited Create; end; class function TK.Neu: TK; begin Result := TK.Create; end;
procedure P; vao a := TK.Create; begin var r: Integer := a.V; begin a.Free; r := 1; end; end;
begin P; end.
