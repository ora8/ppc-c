program equality;
type
  TPoint = record X, Y: Integer; end;
  TPerson = record Name: String; Age: Integer; Tags: array of String; Nick: optional String; end;
  TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty);
  TGrid = array[1..2] of TPoint;
function P(x, y: Integer): TPoint; begin Result.X := x; Result.Y := y; end;
function Same<T: Equatable>(a, b: T): Boolean; begin Result := a = b; end;
val
  a: TPoint := (X: 1; Y: 2);
  b: TPoint := (X: 1; Y: 2);
  anna: TPerson := (Name: 'Anna'; Age: 30; Tags: ('a', 'b'); Nick: 'An');
  anna2: TPerson := (Name: 'Anna'; Age: 30; Tags: ('a', 'b'); Nick: 'An');
  s := Circle(2.0);
  g1: TGrid := ((X: 1; Y: 1), (X: 2; Y: 2));
  g2: TGrid := ((X: 1; Y: 1), (X: 2; Y: 3));
begin
  WriteLn(a = b, ' ', a <> b, ' ', a = P(1, 3), ' ', a = (X: 1; Y: 2));
  WriteLn(anna = anna2);
  anna2.Tags[1] := 'c';
  WriteLn(anna = anna2, ' ', anna.Nick = anna2.Nick);
  WriteLn(s = Circle(2.0), ' ', s = Circle(3.0), ' ', s = Empty, ' ', Rect(1, 2) = Rect(1, 2));
  WriteLn(g1 = g2, ' ', g1 = g1);
  WriteLn(Same(a, b), ' ', Same(s, Empty));
end.
