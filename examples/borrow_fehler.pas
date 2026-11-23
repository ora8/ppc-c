program BorrowFehler;
{ Absichtlich fehlerhaft: jede markierte Zeile wird vom Borrow Checker
  abgelehnt. Uebersetzen mit:  ./ppc examples/borrow_fehler.pas }
type
  TKonto = class
    Saldo: Integer;
  end;
  PInt = ^Integer;

var
  Global: borrowed PInt;

procedure Schliesse(K: TKonto);          { K ist nur ausgeliehen }
begin
  K.Free;                                { FEHLER 1: Ausleihe freigeben }
end;

procedure Merke;
var zahl: Integer;
begin
  zahl := 42;
  Global := @zahl;                       { FEHLER 2: Stackreferenz entkommt }
end;

procedure Tausche(var A, B: Integer);
var t: Integer;
begin t := A; A := B; B := t; end;

heap
  a, b: TKonto;
var
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
  a := TKonto.Create;                    { WARNUNG: erstes Objekt geht verloren }
  a.Free;
end.
