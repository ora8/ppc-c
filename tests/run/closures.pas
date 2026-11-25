program closures;
type
  TCounter = class
    Count: Integer;
    Step: Integer;
    procedure Run(n: Integer);
  end;

procedure ForEach(a: array of Integer; Visit: reference to procedure(x: Integer));
begin
  for val x in a do Visit(x);
end;

function CountIf(a: array of Integer; Keep: reference to function(x: Integer): Boolean): Integer;
begin
  Result := 0;
  for val x in a do
    if Keep(x) then Inc(Result);
end;

function IsEven(x: Integer): Boolean;
begin
  Result := not Odd(x);
end;

procedure TCounter.Run(n: Integer);
val a: array of Integer := default;
begin
  SetLength(a, n);
  for val i := 0 to n - 1 do a[i] := i + 1;
  ForEach(a, procedure(x: Integer) begin Count := Count + x * Step; end);
end;

procedure Demo(Limit: Integer);
val
  a: array of Integer := (1, 5, 8, 12, 3);
  Sum := 0;
  Prefix := 'item ';
  procedure Show(x: Integer);
  begin
    WriteLn(Prefix, x);
  end;
begin
  ForEach(a, procedure(x: Integer) begin Sum := Sum + x; end);
  WriteLn('sum ', Sum);
  WriteLn('above ', Limit, ': ', CountIf(a, function(x: Integer): Boolean begin Result := x > Limit; end));
  WriteLn('even: ', CountIf(a, IsEven));
  ForEach(a, Show);
  val Twice: reference to function(x: Integer): Integer := function(x: Integer): Integer begin Result := x * 2 + Sum; end;
  begin
    WriteLn(Twice(1));
    Sum := 100;
    WriteLn(Twice(1));
  end;
end;

begin
  Demo(4);
  vao c := TCounter.Create;
  begin
    c.Step := 10;
    c.Run(3);
    WriteLn(c.Count);
  end;
end.
