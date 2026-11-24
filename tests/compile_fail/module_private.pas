{ EXPECT: identifier not found "Secret" }
program T;
import Shapes;
vao b := TBox.Create;
begin b.W := 2; WriteLn(Area(b)); WriteLn(Secret); end.
