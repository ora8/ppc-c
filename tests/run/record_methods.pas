program RecordMethods;
{ records with methods: value types with behaviour, without class and heap }
type
  TVector = record
    X, Y: Real;
    procedure Put(AX, AY: Real);
    procedure Scale(F: Real);                 { changes the record itself }
    function Len: Real;
    function Plus(B: TVector const): TVector;    { returns a new value }
    function AsText: String;
    property Width: Real read X write X;
  end;

  TRectangle = record
    Corner, Magnitude: TVector;                       { Records in Records }
    function Area: Real;
    function Contains(P: TVector const): Boolean;
  end;

procedure TVector.Put(AX, AY: Real); begin X := AX; Y := AY; end;
procedure TVector.Scale(F: Real); begin X := X * F; Y := Y * F; end;
function TVector.Len: Real; begin Result := Sqrt(Sqr(X) + Sqr(Y)); end;
function TVector.Plus(B: TVector const): TVector;
begin Result.Put(X + B.X, Y + B.Y); end;
function TVector.AsText: String;
val sx, sy: String := default;
begin
  Str(X:0:1, sx); Str(Y:0:1, sy);
  Result := '(' + sx + ', ' + sy + ')';
end;

function TRectangle.Area: Real; begin Result := Magnitude.X * Magnitude.Y; end;
function TRectangle.Contains(P: TVector const): Boolean;
begin
  Result := (P.X >= Corner.X) and (P.X <= Corner.X + Magnitude.X) and
            (P.Y >= Corner.Y) and (P.Y <= Corner.Y + Magnitude.Y);
end;

function vec(AX, AY: Real): TVector;          { "constructor" as a function }
begin Result.Put(AX, AY); end;

val
  a, b, c: TVector := default;
  r: TRectangle := default;
  points: array[1..3] of TVector := default;
  i: Integer := default;
begin
  a.Put(3, 4);
  WriteLn('a = ', a.AsText, ', Len ', a.Len:0:1);
  b := a;                                         { CopyOf }
  b.Scale(2);
  WriteLn('a = ', a.AsText, ', b = ', b.AsText, '  (b is a copy)');
  c := a.Plus(b);
  WriteLn('a + b = ', c.AsText);
  WriteLn('Len of vec(6, 8): ', vec(6, 8).Len:0:1);   { method on a result value }
  WriteLn('a.Plus(b).Plus(a) = ', a.Plus(b).Plus(a).AsText);
  a.Width := 10;
  WriteLn('after a.Width := 10: ', a.AsText, ', Width ', a.Width:0:1);

  r.Corner.Put(0, 0);
  r.Magnitude.Put(4, 3);
  WriteLn('Area ', r.Area:0:1, ', contains (2,2): ', r.Contains(vec(2, 2)), ', contains (5,1): ', r.Contains(vec(5, 1)));

  for i := 1 to 3 do points[i].Put(i, i * i);
  for i := 1 to 3 do points[i].Scale(10);
  for i := 1 to 3 do Write(points[i].AsText, ' ');
  WriteLn;
  r.Magnitude.Scale(2);
  WriteLn('with with: ', r.Magnitude.AsText);
end.
