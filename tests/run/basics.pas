program Basics;
const
  N = 10;
  Greeting = 'Hallo';
  Primes: array[1..5] of Integer = (2, 3, 5, 7, 11);
type
  TColor = (Red, Green, Blue);
  TPoint = record
    X, Y: Integer;
  end;
  PNode = ^TNode;
  TNode = record
    Value: Integer;
    Next: PNode;
  end;
var
  i, sum: Integer;
  r: Real;
  s: String;
  c: Char;
  col: TColor;
  p: TPoint;
  a: array[0..N - 1] of Integer;
  m: array[1..3, 1..3] of Integer;

function Fact(n: Integer): Int64;
begin
  if n <= 1 then Result := 1 else Result := n * Fact(n - 1);
end;

procedure Swap(var a, b: Integer);
var t: Integer;
begin
  t := a; a := b; b := t;
end;

function SumTo(n: Integer): Integer;
var acc: Integer;
  procedure Add(k: Integer);
  begin
    acc := acc + k;
  end;
var k: Integer;
begin
  acc := 0;
  for k := 1 to n do Add(k);
  SumTo := acc;
end;

begin
  sum := 0;
  for i := 1 to N do sum := sum + i;
  WriteLn('Summe 1..', N, ' = ', sum);
  WriteLn(Greeting + ', Welt!', ' Laenge=', Length(Greeting));
  WriteLn('10! = ', Fact(10));
  r := 22 / 7;
  WriteLn('r = ', r:0:4);
  WriteLn(r);
  i := 3; sum := 4; Swap(i, sum);
  WriteLn('swap: ', i, ' ', sum);
  for col := Red to Blue do Write(col, ' ');
  WriteLn;
  p.X := 3; p.Y := 4;
  with p do WriteLn('Punkt (', X, ',', Y, ')');
  for i := 0 to N - 1 do a[i] := i * i;
  WriteLn('a[7] = ', a[7]);
  for i := 1 to 3 do m[i, i] := i;
  WriteLn('m[2,2] = ', m[2][2]);
  s := 'abc';
  s := s + 'def';
  s[1] := 'X';
  WriteLn(s, ' pos(de)=', Pos('de', s), ' copy=', Copy(s, 2, 3));
  c := 'q';
  if c in ['a'..'z'] then WriteLn(c, ' ist klein, gross: ', UpCase(c));
  case sum of
    1, 2: WriteLn('eins/zwei');
    3..5: WriteLn('drei bis fuenf');
  else
    WriteLn('sonst');
  end;
  i := 0;
  repeat Inc(i, 2) until i >= 7;
  WriteLn('repeat: ', i);
  i := 0;
  while True do begin
    Inc(i);
    if i = 3 then Continue;
    if i > 5 then Break;
    Write(i, ' ');
  end;
  WriteLn;
  WriteLn('SumTo(100) = ', SumTo(100));
  for i := 1 to 5 do Write(Primes[i], ' ');
  WriteLn;
  WriteLn('17 div 5 = ', 17 div 5, ', 17 mod 5 = ', 17 mod 5, ', odd(7)=', Odd(7));
  WriteLn(IntToStr(42) + '!', ' ', 1 shl 10, ' ', High(Integer));
end.
