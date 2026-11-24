program GenericConstraints;
{ Einschraenkungen (Stufe 3): Comparable, Equatable, Clone, class, Basisklasse }
type
  TTier = class
    Name: String;
    constructor Create(AName: String const);
    function Clone: TTier; const; auto;
    function Laut: String; virtual;
  end;
  THund = class(TTier)
    function Laut: String; override;
  end;
  TPaar<K: Comparable; V> = record
    Schluessel: K;
    Wert: V;
  end;
  TZoo<T: TTier> = class
    Tiere: array[1..5] of optional T;
    Anzahl: Integer;
    procedure Add(vao t: T);
    procedure Alle;
  end;

constructor TTier.Create(AName: String const); begin inherited Create; Name := AName; end;
function TTier.Laut: String; begin Result := '...'; end;
function THund.Laut: String; begin Result := 'Wuff'; end;

procedure TZoo<T>.Add(vao t: T); begin Inc(Anzahl); Tiere[Anzahl] := t; end;
procedure TZoo<T>.Alle;
begin
  for val i := 1 to Anzahl do
    if Tiere[i] is some t then WriteLn('  ', t.Name, ': ', t.Laut);   { Methoden von TTier erlaubt }
end;

function Max<T: Comparable>(a, b: T): T;
begin if a > b then Result := a else Result := b; end;

function Gleich<T: Equatable>(a, b: T): Boolean;
begin Result := a = b; end;

function Doppel<T: Clone>(x: T): T;
begin Result := x.Clone; end;

function Name<T: class>(x: T): String;
begin Result := x.ClassName; end;

val p: TPaar<String, Integer> := (Schluessel: 'a'; Wert: 1);
vao zoo := TZoo<THund>.Create;
vao t := TTier.Create('Mia');
begin
  WriteLn(Max(3, 9), ' ', Max('x', 'y'), ' ', Gleich(2, 2), ' ', Gleich('a', 'b'));
  zoo.Add(THund.Create('Rex'));
  zoo.Alle;
  vao k := Doppel(t);
  begin
    k.Name := 'Mia2';
    WriteLn(t.Name, ' ', k.Name, ' ', Name(k));
  end;
  WriteLn(p.Schluessel, ' ', p.Wert);
end.
