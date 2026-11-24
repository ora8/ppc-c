program GenericRoutines;
{ Generische Funktionen und Prozeduren (Stufe 2): Typ aus den Argumenten oder ausdruecklich }
type
  TKunde = class
    Name: String;
    constructor Create(AName: String const);
  end;
  TPaar<A, B> = record
    Erstes: A;
    Zweites: B;
  end;
constructor TKunde.Create(AName: String const); begin inherited Create; Name := AName; end;

function Max<T>(a, b: T): T;
begin
  if a > b then Result := a else Result := b;
end;

procedure Tausche<T>(var a, b: T);
val h: T := a;
begin
  a := b;
  b := h;
end;

function Paar<A, B>(x: A; y: B): TPaar<A, B>;
begin
  Result.Erstes := x;
  Result.Zweites := y;
end;

function Erstes<A, B>(p: TPaar<A, B>): A;
begin Result := p.Erstes; end;

function Fakultaet<T>(n: T): T;          { rekursiv }
begin
  if n <= 1 then Result := 1 else Result := n * Fakultaet(n - 1);
end;

function Name<T>(k: T): String;
begin Result := k.Name; end;

val
  i := 3;
  j := 9;
  s := 'Anna';
  t := 'Bert';
vao k := TKunde.Create('Carla');
begin
  WriteLn(Max(3, 7), ' ', Max('Anna', 'Bert'), ' ', Max(2.5, 1.0):0:1);
  WriteLn(Max(3, 7.5):0:1, ' ', Max<Real>(1, 2):0:1);
  Tausche(i, j);
  Tausche(s, t);
  WriteLn(i, ' ', j, ' ', s, ' ', t);
  val p := Paar('x', 42);
  begin
    WriteLn(p.Erstes, ' ', p.Zweites, ' ', Erstes(p));
  end;
  WriteLn(Fakultaet(10), ' ', Name(k));
end.
