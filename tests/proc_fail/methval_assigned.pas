{* EXPECT: a method value can only be passed as an argument or stored in a local 'reference to' variable, and its object must be a variable *}
program T; type TP = class N: Integer; procedure Show(x: Integer); end; TBox = class Inner: TP; constructor Create; end;
procedure TP.Show(x: Integer); begin WriteLn(x); end; constructor TBox.Create; begin inherited Create; Inner := TP.Create; end;
procedure Q; vao b := TBox.Create; val f: reference to procedure(x: Integer) := procedure(x: Integer) begin end; begin f := b.Inner.Show; f(1); end; begin end.
