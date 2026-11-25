{ EXPECT: "Show" needs 1 argument }
program T; type TP = class N: Integer; procedure Show(x: Integer); procedure Tick; end; procedure TP.Show(x: Integer); begin WriteLn(x); end; procedure TP.Tick; begin end;
procedure Q; vao p := TP.Create; begin p.Show; end; begin end.
