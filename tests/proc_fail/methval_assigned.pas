{* EXPECT: a method value can only be passed directly as an argument *}
program T; type TP = class N: Integer; procedure Show(x: Integer); procedure Tick; end; procedure TP.Show(x: Integer); begin WriteLn(x); end; procedure TP.Tick; begin end;
procedure Q; vao p := TP.Create; val f: reference to procedure(x: Integer) := procedure(x: Integer) begin end; begin f := p.Show; f(1); end; begin end.
