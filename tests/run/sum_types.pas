program sum_types;
type
  TShape = (
    Circle(Radius: Real),
    Rect(W, H: Real),
    Named(Name: String; Sides: Integer),
    Empty
  );

function Area(s: TShape): Real;
begin
  case s of
    Circle(r): Result := 3.14159 * r * r;
    Rect(w, h): Result := w * h;
    Named(_, n): Result := n;
    Empty: Result := 0;
  end;
end;

function Describe(s: TShape): String;
begin
  case s of
    Circle: Result := 'circle';
    Named(nm, _): Result := 'named ' + nm;
  else
    Result := 'other';
  end;
end;

function Make(i: Integer): TShape;
begin
  case i of
    0: Result := Circle(1.0);
    1: Result := Rect(2, 3);
    2: Result := Named('hexagon', 6);
  else
    Result := Empty;
  end;
end;

val
  a: array of TShape := (Circle(2.0), TShape.Rect(3.0, 4.0), Named('tri', 3), Empty);
  s := Named('square', 4);
begin
  for val x in a do WriteLn(Area(x):0:2, ' ', Describe(x));
  if s is Named(n, k) then WriteLn(n, ' ', k) else WriteLn('no');
  if a[0] is Rect(w, h) then WriteLn(w) else WriteLn('not a rect');
  if s is Empty then WriteLn('empty') else WriteLn('not empty');
  case s of
    Named(var n, var k): begin n := n + '!'; Inc(k); end;
  else
  end;
  if s is Named(n, k) then WriteLn(n, ' ', k);
  s := Empty;
  WriteLn(Describe(s));
  for val i := 0 to 3 do WriteLn(Describe(Make(i)));
  case Make(2) of
    Named(n, _): WriteLn('made ', n);
  else
  end;
end.
