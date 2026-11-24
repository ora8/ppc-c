program ConstParams;
{ 'T const': unchangeable; as a parameter by reference, without a copy }
type
  TVector = record
    X, Y: Real;
    procedure Scale(F: Real);
    function Len: Real; const;
  end;
  TBig = array[1..1000] of Integer;
procedure TVector.Scale(F: Real); begin X := X * F; Y := Y * F; end;
function TVector.Len: Real; const; begin Result := Sqrt(X * X + Y * Y); end;

function Sum(A: TBig const): Integer;     { no copy of 4000 bytes }
begin
  Result := 0;
  for val i := 1 to 1000 do Result := Result + A[i];
end;

procedure Show(S: String const; V: TVector const);
begin
  WriteLn(S, ': ', V.Len:0:1);                { functions are allowed }
end;

function Twice(N: Integer const): Integer; begin Result := N * 2; end;

procedure P(n: Integer);
const
  Max = 100;                    { compile-time constant }
val
  Square: Integer const := n * n; { unchangeable, value at run time }
  Text: String const := 'n=' + IntToStr(n);
  v: TVector := default;
begin
  v.X := 3; v.Y := 4;
  WriteLn(Max, ' ', Square, ' ', Text);
  Show('v', v);
  Show('temporary', v);
  WriteLn('Twice(Square + 1) = ', Twice(Square + 1));   { expression as a const argument }
  val w: TVector const := v;    { unchangeable copy in the block }
  begin
    WriteLn('w.Len = ', w.Len:0:1);
  end;
end;

val
  g: TBig := default;
begin
  for val i := 1 to 1000 do g[i] := i;
  WriteLn('Sum = ', Sum(g));
  P(7);
end.
