{* EXPECT: case expression does not handle the variant Empty of "TShape" (add it or an 'else') *}
program T;
type TShape = (Circle(Radius: Real), Empty);
val s: TShape := Empty;
begin WriteLn(case s of Circle(r) when r > 0: 1; Circle: 2 end); end.
