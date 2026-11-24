program HeapValues;
{ Werte auf dem Heap: mit 'vao' deklariert, im Code benutzt wie ein normaler Wert und
  am Ende des Gueltigkeitsbereichs automatisch freigegeben (wie Box<T> in Rust) }
type
  TVektor = record
    X, Y: Real;
    procedure Skaliere(F: Real);
  end;
  TFeld = array[1..5] of Integer;

procedure TVektor.Skaliere(F: Real); begin X := X * F; Y := Y * F; end;

procedure Verdopple(var N: Integer); begin N := N * 2; end;
function Quadrat(N: Integer): Integer; begin Result := N * N; end;

procedure Lokal;
vao
  Zaehler: Integer := 100;                   { Heapwert in einer Prozedur }
begin
  Inc(Zaehler);
  WriteLn('lokal: ', Zaehler);
end;                                         { Zaehler wird hier freigegeben }

procedure Frueh;
vao
  Puffer: TFeld := default;
begin
  Puffer[1] := 7;
  WriteLn('frueh: ', Puffer[1]);
  Puffer.Free;                               { optional: schon hier freigeben }
  Puffer := (1, 2, 3, 4, 5);                 { ganzer neuer Wert: legt eine neue Zelle an }
  WriteLn('wieder: ', Puffer[1], ' ', Puffer[5]);
end;

vao
  I: Integer := 20;
  S: String := 'Hallo';
  V: TVektor := (X: 3; Y: 4);
  F: TFeld := default;
begin
  I := I + 1;
  Verdopple(I);                   { var-Parameter: Referenz auf den Heapwert }
  WriteLn('I = ', I, ', Quadrat = ', Quadrat(I));

  S := S + ' Welt';
  WriteLn('S = ', S, ' (', Length(S), ' Zeichen)');

  V.Skaliere(2);                  { Methode eines Records auf dem Heap }
  WriteLn('V = (', V.X:0:1, ', ', V.Y:0:1, ')');

  for val k := 1 to 5 do F[k] := k * k;
  WriteLn('F[5] = ', F[5]);

  Lokal;
  Frueh;
end.                              { I, S, V und F werden hier freigegeben }
