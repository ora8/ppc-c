{* EXPECT: a nested pattern after 'is' can only be the whole condition of an 'if' (or use 'case') *}
program T;
type TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty); TItem = (Single(S: TShape), Pair(A, B: TShape), Box(Content: optional Integer));
function F: optional TShape; begin Result := Empty; end; begin if (1 > 0) and (F is some Circle(r)) then WriteLn(1); end.
