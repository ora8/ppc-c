program Endless;
{ loop <statement>: endless loop, left with Break, Exit or try }
type
  TK = class
    N: Integer;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  freed ', N); inherited Destroy; end;

function FirstSquareAbove(g: Integer): Integer;
val i := 1;
begin
  loop
    begin
      if i * i > g then Exit(i * i);
      Inc(i);
    end;
end;

function Check(n: Integer): result Integer;
begin
  if n > 3 then Result := error('too big at ' + IntToStr(n)) else Result := n;
end;

function Sum: result Integer;
val n := 0;
begin
  Result := 0;
  loop
    begin
      Inc(n);
      val x := try Check(n);               { an error ends the loop and the function }
      begin
        Result := ok(x);
      end;
    end;
end;

val
  i := 0;
  raise := 5;                               { 'raise' is no keyword any more }
begin
  loop
    begin
      Inc(i);
      if i = 2 then Continue;
      if i > 4 then Break;
      Write(i, ' ');
    end;
  WriteLn;
  WriteLn('SquareOf: ', FirstSquareAbove(50));
  case Sum of ok v: WriteLn(v); error e: WriteLn('Error: ', e); end;
  i := 0;
  loop
    vao k := TK.Create(i);                  { a new owner in every iteration }
    begin
      Inc(i);
      if i = 3 then Break;
    end;
  WriteLn('raise = ', raise);
end.
