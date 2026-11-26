program WhileDecl;
{* while val i := 0; i < n do ...: the variable belongs to the loop (created once) *}
type TK = class N: Integer; destructor Destroy; override; end;
destructor TK.Destroy; begin WriteLn('freed ', N); inherited Destroy; end;
begin
  while val I := 0; I < 3 do
  begin
    Write(I, ' ');
    Inc(I);
  end;
  WriteLn;
  while vao k := TK.Create; k.N < 2 do Inc(k.N);
  WriteLn('after the loop');
  while val s: String := 'a'; Length(s) < 4 do
    val t := s + 'b';
    begin
      s := t;
    end;
  WriteLn('done');
end.
