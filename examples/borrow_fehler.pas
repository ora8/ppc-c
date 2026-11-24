program BorrowFehler;
{ Absichtlich fehlerhaft: jede markierte Zeile wird vom Borrow Checker
  abgelehnt. Uebersetzen mit:  ./ppc examples/borrow_fehler.pas }
type
  TKonto = class
    Saldo: Integer;
  end;

val
  Global: borrowed optional TKonto;

procedure Schliesse(K: TKonto);          { K ist nur ausgeliehen }
begin
  K.Free;                                { FEHLER 1: Ausleihe freigeben }
end;

procedure Merke;
val konto: TKonto;                       { Stackobjekt }
begin
  konto.Create;
  Global := konto;                       { FEHLER 2: Referenz auf Stackobjekt entkommt }
end;

procedure Tausche(var A, B: Integer);
val t: Integer;
begin t := A; A := B; B := t; end;

var
  a, b: TKonto;
val
  blick: borrowed TKonto;
  x: Integer;
begin
  a := TKonto.Create;
  b := a;                                { Move: a -> b }
  a.Saldo := 100;                        { FEHLER 3: a wurde verschoben }

  blick := b;                            { Ausleihe von b }
  b.Free;
  WriteLn(blick.Saldo);                  { FEHLER 4: Besitzer schon freigegeben }

  b.Free;                                { FEHLER 5: doppeltes Free }

  x := 1;
  Tausche(x, x);                         { FEHLER 6: zwei var-Ausleihen von x }

  a := TKonto.Create;
  a := TKonto.Create;                    { kein Fehler: das erste Objekt wird automatisch freigegeben }
  a.Free;
end.
