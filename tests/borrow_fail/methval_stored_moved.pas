{* EXPECT: 'f' holds a method value of 'p', which was moved away *}
program T; type TP = class N: Integer; procedure Show(x: Integer); end; procedure TP.Show(x: Integer); begin WriteLn(x); end;
procedure Eat(vao p: TP); begin end;
procedure Q; vao p := TP.Create; val f: reference to procedure(x: Integer) := p.Show; begin Eat(p); f(1); end; begin end.
