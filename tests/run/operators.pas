program operators;
type
  TVec = record
    X, Y: Real;
    class operator Add(a, b: TVec): TVec;
    class operator Subtract(a, b: TVec): TVec;
    class operator Multiply(a: TVec; f: Real): TVec;
    class operator Multiply(f: Real; a: TVec): TVec;
    class operator Negative(a: TVec): TVec;
    class operator Equal(a, b: TVec): Boolean;
    function Text: String; const;
  end;
  TMoney = record
    Cents: Int64;
    class operator Add(a, b: TMoney): TMoney;
    class operator LessThan(a, b: TMoney): Boolean;
    class operator GreaterThan(a, b: TMoney): Boolean;
  end;
function V(x, y: Real): TVec; begin Result.X := x; Result.Y := y; end;
class operator TVec.Add(a, b: TVec): TVec; begin Result := V(a.X + b.X, a.Y + b.Y); end;
class operator TVec.Subtract(a, b: TVec): TVec; begin Result := V(a.X - b.X, a.Y - b.Y); end;
class operator TVec.Multiply(a: TVec; f: Real): TVec; begin Result := V(a.X * f, a.Y * f); end;
class operator TVec.Multiply(f: Real; a: TVec): TVec; begin Result := a * f; end;
class operator TVec.Negative(a: TVec): TVec; begin Result := V(-a.X, -a.Y); end;
class operator TVec.Equal(a, b: TVec): Boolean; begin Result := (Abs(a.X - b.X) < 0.001) and (Abs(a.Y - b.Y) < 0.001); end;
function TVec.Text: String; const; begin Result := '(' + FloatToStr(X) + ', ' + FloatToStr(Y) + ')'; end;
class operator TMoney.Add(a, b: TMoney): TMoney; begin Result.Cents := a.Cents + b.Cents; end;
class operator TMoney.LessThan(a, b: TMoney): Boolean; begin Result := a.Cents < b.Cents; end;
class operator TMoney.GreaterThan(a, b: TMoney): Boolean; begin Result := a.Cents > b.Cents; end;
val
  a := V(1, 2);
  b := V(3, 4);
  m: TMoney := (Cents: 150);
  n: TMoney := (Cents: 99);
begin
  WriteLn((a + b).Text, ' ', (b - a).Text, ' ', (a * 3).Text, ' ', (2 * b).Text, ' ', (-a).Text);
  WriteLn((a + b * 2 - a).Text);
  WriteLn(a + b = V(4, 6), ' ', a <> b, ' ', a = V(1.0001, 2));
  WriteLn((m + n).Cents, ' ', m < n, ' ', m > n);
end.
