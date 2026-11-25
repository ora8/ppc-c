{ EXPECT: 'p' is passed as var in the call of 'U' while the method value 'p.Tick' uses it }
program T; type TP = class N: Integer; procedure Show(x: Integer); procedure Tick; end; procedure TP.Show(x: Integer); begin WriteLn(x); end; procedure TP.Tick; begin end;
procedure U(f: reference to procedure; var x: TP); begin f; end; procedure Q; vao p := TP.Create; begin U(p.Tick, p); end; begin end.
