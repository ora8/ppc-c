program WhileDecl;
{ while val i := 0; i < n do ...: die Variable gehoert zur Schleife (einmal angelegt) }
type TK = class N: Integer; destructor Destroy; override; end;
destructor TK.Destroy; begin WriteLn('frei ', N); inherited Destroy; end;
begin
  while val I := 0; I < 3 do
  begin
    Write(I, ' ');
    Inc(I);
  end;
  WriteLn;
  while vao k := TK.Create; k.N < 2 do Inc(k.N);
  WriteLn('nach der Schleife');
  while val s: String := 'a'; Length(s) < 4 do
    val t := s + 'b';
    begin
      s := t;
    end;
  WriteLn('fertig');
end.
