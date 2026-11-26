{* EXPECT: the pattern 'Circle(...)' needs an enumeration with data or an object, not "optional Integer" *}
program T;
type TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty); TItem = (Single(S: TShape), Pair(A, B: TShape), Box(Content: optional Integer));
procedure P(it: TItem); begin case it of Box(Circle(r)): WriteLn(r); else end; end; begin end.
