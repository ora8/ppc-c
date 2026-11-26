{ EXPECT: 'var' names are not possible after 'some': its value is read-only }
program T;
type TShape = (Circle(Radius: Real), Rect(W, H: Integer), Empty); TItem = (Single(S: TShape), Pair(A, B: TShape), Box(Content: optional Integer));
procedure P(var it: TItem); begin case it of Box(some var n): n := 1; else end; end; begin end.
