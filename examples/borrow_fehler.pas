program BorrowFehler;
{ Absichtlich fehlerhaft: jede markierte Zeile wird vom Borrow Checker
  abgelehnt. Uebersetzen mit:  ./ppc examples/borrow_fehler.pas }
type
  TKonto = class
    Saldo: Integer;
  end;

var
  Global: optional TKonto := none;

procedure Schliesse(K: TKonto);          { K ist nur ausgeliehen }
begin
  K.Free;                                { FEHLER 1: Ausleihe freigeben }
end;

procedure Merke;
vao konto := TKonto.Create;              { wird am Ende von Merke freigegeben }
begin
  Global := konto;                       { FEHLER 2: Referenz auf lokalen Besitzer entkommt }
end;

procedure Tausche(var A, B: Integer);
val t: Integer := default;
begin t := A; A := B; B := t; end;

vao
  a := TKonto.Create;
  b: TKonto := a;                        { Move: a -> b }
var
  blick: TKonto := b;                    { Ausleihe von b (besitzt nicht) }
val
  x: Integer := default;
begin
  a.Saldo := 100;                        { FEHLER 3: a wurde verschoben }
  b.Free;
  WriteLn(blick.Saldo);                  { FEHLER 4: Besitzer schon freigegeben }

  b.Free;                                { FEHLER 5: doppeltes Free }

  x := 1;
  Tausche(x, x);                         { FEHLER 6: zwei var-Ausleihen von x }

  a := TKonto.Create;
  a := TKonto.Create;                    { kein Fehler: das erste Objekt wird automatisch freigegeben }
  a.Free;
end.
