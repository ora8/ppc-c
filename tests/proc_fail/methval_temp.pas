{ EXPECT: the object of a method value must be a variable }
program T; type TP = class N: Integer; procedure Show(x: Integer); procedure Tick; end; procedure TP.Show(x: Integer); begin WriteLn(x); end; procedure TP.Tick; begin end;
procedure U(f: reference to procedure(x: Integer)); begin f(1); end; procedure Q; begin U(TP.Create.Show); end; begin end.
