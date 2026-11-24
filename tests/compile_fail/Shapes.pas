module Shapes;
interface
type TBox = class W: Integer; procedure Show; end;
function Area(b: TBox): Integer;
implementation
function Secret: Integer; begin Result := 42; end;
procedure TBox.Show; begin WriteLn(W); end;
function Area(b: TBox): Integer; begin Result := b.W * Secret; end;
end.
