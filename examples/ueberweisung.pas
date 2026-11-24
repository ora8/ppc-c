program Ueberweisung;
{ Fehlerbehandlung ohne Exceptions: 'result T of E' (wie Result<T, E> in Rust),
  Fehler weitergeben mit 'try' (wie '?'), auspacken mit case / is ok / ValueOr.
  Uebersetzen mit:  ./ppc examples/ueberweisung.pas }
type
  TFehler = (KeinGeld, UnbekanntesKonto, BetragUngueltig);
  TKonto = class
    Name: String;
    Saldo: Integer;
    constructor Create(AName: String const; ASaldo: Integer);
  end;
  TBank = class
    Konten: array[1..3] of optional TKonto;
    function Suche(Name: String const): result Integer of TFehler;
    function Abheben(Nr, Betrag: Integer): result of TFehler;
    function Ueberweise(Von, Nach: String const; Betrag: Integer): result Integer of TFehler;
  end;

constructor TKonto.Create(AName: String const; ASaldo: Integer);
begin inherited Create; Name := AName; Saldo := ASaldo; end;

function TBank.Suche(Name: String const): result Integer of TFehler;
begin
  Result := error(UnbekanntesKonto);
  for val i := 1 to 3 do
    if Konten[i] is some k then
      if k.Name = Name then Result := i;       { ein Wert ist automatisch ok(i) }
end;

function TBank.Abheben(Nr, Betrag: Integer): result of TFehler;
begin
  Result := ok;
  if Betrag <= 0 then Result := error(BetragUngueltig)
  else if Konten[Nr] is some k then
    if k.Saldo < Betrag then Result := error(KeinGeld)
    else k.Saldo := k.Saldo - Betrag;
end;

function TBank.Ueberweise(Von, Nach: String const; Betrag: Integer): result Integer of TFehler;
begin
  val a := try Suche(Von);                     { Fehler: Ueberweise endet hier }
  val b := try Suche(Nach);
  begin
    try Abheben(a, Betrag);                    { Anweisung: nur der Fehler zaehlt }
    if Konten[b] is some k then
    begin
      k.Saldo := k.Saldo + Betrag;
      Result := k.Saldo;                       { neuer Saldo des Empfaengers }
    end;
  end;
end;

function Text(f: TFehler): String;
begin
  case f of
    KeinGeld: Result := 'nicht genug Geld';
    UnbekanntesKonto: Result := 'Konto unbekannt';
    BetragUngueltig: Result := 'Betrag ungueltig';
  end;
end;

procedure Versuche(Bank: TBank; Von, Nach: String const; Betrag: Integer);
begin
  Write(Von, ' -> ', Nach, ' ', Betrag, ': ');
  case Bank.Ueberweise(Von, Nach, Betrag) of
    ok saldo: WriteLn('ok, ', Nach, ' hat jetzt ', saldo);
    error f:  WriteLn('Fehler: ', Text(f));
  end;
end;

vao bank := TBank.Create;
begin
  bank.Konten[1] := TKonto.Create('Anna', 100);
  bank.Konten[2] := TKonto.Create('Bert', 20);
  Versuche(bank, 'Anna', 'Bert', 30);
  Versuche(bank, 'Bert', 'Anna', 500);
  Versuche(bank, 'Anna', 'Carla', 10);
  Versuche(bank, 'Anna', 'Bert', -5);

  if bank.Suche('Bert') is ok nr then WriteLn('Bert hat Konto Nr. ', nr);
  WriteLn('Carla: ', bank.Suche('Carla').ValueOr(0));
  if bank.Abheben(1, 1000).IsError then WriteLn('1000 abheben geht nicht');
end.
