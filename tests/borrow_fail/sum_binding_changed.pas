{* EXPECT: 's' was changed at line 2 while 'r' was borrowing it *}
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin case s of Circle(var r): begin s := Empty; r := 2; end; else end; end.
