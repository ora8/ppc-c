program Results;
{ Fehler als Rueckgabewert: 'result T' (wie Result<T, E> in Rust) }
type
  TFehler = (KeinFehler, DurchNull, ZuGross);
  TPunkt = record
    X, Y: Integer;
  end;
  TZaehler = class
    N: Integer;
    destructor Destroy; override;
  end;

destructor TZaehler.Destroy; begin WriteLn('  frei: Zaehler ', N); inherited Destroy; end;

function Teile(a, b: Integer): result Integer;          { Fehler: String }
begin
  if b = 0 then Result := error('Division durch 0')
  else Result := a div b;                               { Wert = ok(...) }
end;

function Pruefe(n: Integer): result of TFehler;         { kein Wert, nur Erfolg oder Fehler }
begin
  if n > 100 then Result := error(ZuGross) else Result := ok;
end;

function Rechne(a, b, c: Integer): result Integer;
vao z := TZaehler.Create;                               { wird auch beim Fehler freigegeben }
begin
  val q := try Teile(a, b);                             { Fehler -> sofort zurueck }
  begin
    z.N := q;
    Result := try Teile(q, c);
    Result := ok(q * 10);
  end;
end;

function Kette(n: Integer): result of TFehler;
begin
  try Pruefe(n);                                        { Anweisung: nur der Fehler zaehlt }
  try Pruefe(n * 2);
  WriteLn('  Kette ok fuer ', n);
end;

function Punkt(ok_: Boolean): result TPunkt;
begin
  if ok_ then Result := (X: 1; Y: 2) else Result := error('kein Punkt');
end;

val r := Teile(10, 2);
begin
  if r is ok v then WriteLn('10 / 2 = ', v);
  case Teile(1, 0) of
    ok v: WriteLn('Wert ', v);
    error e: WriteLn('Fehler: ', e);
  end;
  WriteLn('ValueOr: ', Teile(7, 0).ValueOr(-1), ' ', Teile(7, 7).ValueOr(-1));
  WriteLn('IsOk: ', Teile(1, 1).IsOk, ', IsError: ', Teile(1, 0).IsError);
  WriteLn('-- try');
  case Rechne(100, 5, 2) of ok v: WriteLn('Rechne = ', v); error e: WriteLn('Fehler: ', e); end;
  case Rechne(100, 0, 2) of ok v: WriteLn('Rechne = ', v); error e: WriteLn('Fehler: ', e); end;
  case Rechne(100, 5, 0) of ok v: WriteLn('Rechne = ', v); error e: WriteLn('Fehler: ', e); end;
  WriteLn('-- ohne Wert');
  if Kette(10) is ok then WriteLn('  ok');
  if Kette(60) is error f then WriteLn('  Fehler ', Ord(f));
  case Pruefe(500) of
    ok: WriteLn('  passt');
    error f: WriteLn('  zu gross: ', f = ZuGross);
  end;
  WriteLn('-- Record');
  if Punkt(True) is ok p then WriteLn('  (', p.X, ',', p.Y, ')');
  if Punkt(False) is error e then WriteLn('  ', e);
end.
