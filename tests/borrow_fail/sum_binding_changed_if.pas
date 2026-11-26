{* EXPECT: 's' was changed at line 2 while 'r' was borrowing it *}
program T; type TShape = (Circle(Radius: Real), Rect(W, H: Real), Empty); val s: TShape := Empty; begin if s is Circle(r) then begin s := Empty; WriteLn(r); end; end.
