program HeapValues;
{ Werte auf dem Heap: im Abschnitt 'var' deklariert, mit T.Create erzeugt,
  mit .Free freigegeben, im Code benutzt wie ein normaler Wert (wie Box<T>) }
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
var
  Zaehler: Integer;               { Heapwert in einer Prozedur }
begin
  Zaehler := Integer.Create(100);
  Inc(Zaehler);
  WriteLn('lokal: ', Zaehler);
  Zaehler.Free;
end;

var
  I: Integer;
  S: String;
  V: TVektor;
  F: TFeld;
val
  k: Integer;
begin
  I := Integer.Create;            { Heap-Zelle anlegen (mit 0 belegt) }
  I := 20;                        { Wert speichern }
  I := I + 1;
  Verdopple(I);                   { var-Parameter: Referenz auf den Heapwert }
  WriteLn('I = ', I, ', Quadrat = ', Quadrat(I));

  S := String.Create('Hallo');    { mit Anfangswert }
  S := S + ' Welt';
  WriteLn('S = ', S, ' (', Length(S), ' Zeichen)');

  V := TVektor.Create;
  V.X := 3; V.Y := 4;
  V.Skaliere(2);                  { Methode eines Records auf dem Heap }
  WriteLn('V = (', V.X:0:1, ', ', V.Y:0:1, ')');

  F := TFeld.Create;
  for k := 1 to 5 do F[k] := k * k;
  WriteLn('F[5] = ', F[5]);

  Lokal;

  I.Free;                         { Heapwerte von Hand freigeben }
  S.Free;
  V.Free;
  F.Free;
end.
