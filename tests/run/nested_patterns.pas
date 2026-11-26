program NestedPatterns;
{ nested patterns: patterns inside patterns, constants, some/none/ok/error inside, in statements, if and expressions }
type
  TColor = (Red, Green, Blue);
  TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty);
  TItem = (
    Single(S: TShape),
    Pair(A, B: TShape),
    Box(Content: optional Integer),
    Paint(C: TColor; Name: String),
    Nothing
  );
  TFigure = class X: Integer; constructor Create(AX: Integer); end;
  TCircle = class(TFigure) end;
constructor TFigure.Create(AX: Integer); begin inherited Create; X := AX; end;

procedure Show(it: TItem);
begin
  case it of
    Single(Circle(r)) when r > 5: WriteLn('single big circle ', r:0:1);
    Single(Circle(r)): WriteLn('single circle ', r:0:1);
    Single(Rect(0, h)): WriteLn('flat rect, height ', h);
    Single(Rect(w, h)): WriteLn('rect ', w, 'x', h);
    Single(Empty): WriteLn('single empty');
    Pair(Empty, Empty): WriteLn('two empties');
    Pair(Circle(a), Circle(b)): WriteLn('two circles ', a + b:0:1);
    Pair(_, Empty): WriteLn('pair ending empty');
    Box(some 0): WriteLn('box with zero');
    Box(some n) when n < 0: WriteLn('box with negative ', n);
    Box(some n): WriteLn('box with ', n);
    Box(none): WriteLn('empty box');
    Paint(Red, n): WriteLn('red paint ', n);
    Paint(_, 'sky'): WriteLn('sky paint');
    Paint(c, n): WriteLn('paint ', Ord(c), ' ', n);
  else WriteLn('something else');
  end;
end;

procedure Fix(var it: TItem);
begin
  case it of
    Single(Rect(var w, 0)): w := 99;
  else
  end;
end;

function Size(it: TItem): Real;
begin
  Result := case it of
    Single(Circle(r)): r;
    Single(Rect(w, h)) when w = h: w;
    Single(Rect(w, h)): w + h;
    Pair(Circle(a), Circle(b)): a + b;
    Box(some n) when n > 0: n;
    Box(some 0): 0;
    Pair(Empty, Empty), Nothing: 0;
  else -1
  end;
end;

function Find(k: Integer): optional TShape;
begin if k > 0 then Result := Circle(k * 1.5) else if k = 0 then Result := Empty else Result := none; end;

function Divide(a, b: Integer): result Integer;
begin if b = 0 then Result := error('div by zero') else Result := ok(a div b); end;

function Pick(k: Integer): optional TFigure;
begin if k = 1 then Result := TCircle.Create(1) else if k = 2 then Result := TFigure.Create(2) else Result := none; end;

val it: TItem := Single(Rect(4, 0));
vao f := Pick(1);
begin
  Show(Single(Circle(9.0))); Show(Single(Circle(2.0))); Show(Single(Rect(0, 7))); Show(Single(Rect(3, 4)));
  Show(Single(Empty)); Show(Pair(Empty, Empty)); Show(Pair(Circle(1.0), Circle(2.5))); Show(Pair(Rect(1, 1), Empty));
  Show(Pair(Rect(1, 1), Rect(2, 2)));
  Show(Box(0)); Show(Box(-3)); Show(Box(8)); Show(Box(none));
  Show(Paint(Red, 'rose')); Show(Paint(Blue, 'sky')); Show(Paint(Green, 'leaf')); Show(Nothing);
  Fix(it); Show(it);
  for val k := -1 to 2 do
    case Find(k) of
      some Circle(r): WriteLn(k, ': found circle ', r:0:1);
      some Empty: WriteLn(k, ': found empty');
      some s: WriteLn(k, ': found other');
      none: WriteLn(k, ': nothing');
    end;
  case Divide(10, 2) of
    ok 5: WriteLn('exactly five');
    ok v: WriteLn('ok ', v);
    error e: WriteLn(e);
  end;
  case Divide(1, 0) of
    ok v: WriteLn(v);
    error 'div by zero': WriteLn('division by zero');
    error e: WriteLn('other error ', e);
  end;
  case f of
    some TCircle(c): WriteLn('a circle figure at ', c.X);
    some g: WriteLn('a figure');
    none: WriteLn('no figure');
  end;
  WriteLn(Size(Single(Circle(1.5))):0:1, ' ', Size(Single(Rect(2, 2))):0:1, ' ', Size(Single(Rect(2, 3))):0:1, ' ',
          Size(Pair(Circle(1.0), Circle(2.0))):0:1, ' ', Size(Box(4)):0:1, ' ', Size(Box(0)):0:1, ' ', Size(Nothing):0:1, ' ', Size(Box(none)):0:1);
  for val k := -1 to 1 do
    WriteLn(case Find(k) of some Circle(r): 'circle ' + FloatToStr(r); some Empty: 'empty'; some _: 'other'; none: 'none' end);
  WriteLn(case Divide(9, 3) of ok 3: 'three'; ok v: IntToStr(v); error e: e end);
  WriteLn(case f of some TCircle(c): c.X; some g: g.X; none: 0 end);
  if Find(3) is some Circle(r) then WriteLn('if: circle ', r:0:1) else WriteLn('if: no circle');
  if it is Single(Rect(w, 0)) then WriteLn('if: flat ', w) else WriteLn('if: not flat');
end.
