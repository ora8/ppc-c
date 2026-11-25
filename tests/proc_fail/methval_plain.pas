{ EXPECT: a method value refers to its object and cannot be stored as "procedure(Integer)" }
program T; type TP = class N: Integer; procedure Show(x: Integer); procedure Tick; end; procedure TP.Show(x: Integer); begin WriteLn(x); end; procedure TP.Tick; begin end;
type TH = procedure(x: Integer); procedure U(f: TH); begin end; procedure Q; vao p := TP.Create; begin U(p.Show); end; begin end.
