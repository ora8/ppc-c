{ EXPECT: 7 error(s), compilation aborted }
program E2;
type
  TFoo = class
    V: Integer;
    procedure Zeige;
  end;
procedure TFoo.Zeige; begin WriteLn(V + 'x'); end;

function F(n: Integer): Integer;
val s: String := 5;
val wert := 3;
begin
  Result := n * 2;
  Result := 'text';
  Unbekannt(3);
  WriteLn(wert + 1);
end;

val i := 1;
begin
  i := 'abc';
  WriteLn(F(1, 2));
  if i then WriteLn('x');
  WriteLn(i);
end.
