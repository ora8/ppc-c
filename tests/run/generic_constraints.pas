program GenericConstraints;
{ constraints (stage 3): Comparable, Equatable, Clone, class, base class }
type
  TAnimal = class
    Name: String;
    constructor Create(AName: String const);
    function Clone: TAnimal; const; auto;
    function Sound: String; virtual;
  end;
  TDog = class(TAnimal)
    function Sound: String; override;
  end;
  TPair<K: Comparable; V> = record
    Keys: K;
    Value: V;
  end;
  TZoo<T: TAnimal> = class
    Animals: array[1..5] of optional T;
    Size: Integer;
    procedure Add(vao t: T);
    procedure ShowAll;
  end;

constructor TAnimal.Create(AName: String const); begin inherited Create; Name := AName; end;
function TAnimal.Sound: String; begin Result := '...'; end;
function TDog.Sound: String; begin Result := 'Wuff'; end;

procedure TZoo<T>.Add(vao t: T); begin Inc(Size); Animals[Size] := t; end;
procedure TZoo<T>.ShowAll;
begin
  for val i := 1 to Size do
    if Animals[i] is some t then WriteLn('  ', t.Name, ': ', t.Sound);   { methods of TAnimal are allowed }
end;

function Max<T: Comparable>(a, b: T): T;
begin if a > b then Result := a else Result := b; end;

function Equal<T: Equatable>(a, b: T): Boolean;
begin Result := a = b; end;

function CloneOf<T: Clone>(x: T): T;
begin Result := x.Clone; end;

function Name<T: class>(x: T): String;
begin Result := x.ClassName; end;

val p: TPair<String, Integer> := (Keys: 'a'; Value: 1);
vao zoo := TZoo<TDog>.Create;
vao t := TAnimal.Create('Mia');
begin
  WriteLn(Max(3, 9), ' ', Max('x', 'y'), ' ', Equal(2, 2), ' ', Equal('a', 'b'));
  zoo.Add(TDog.Create('Rex'));
  zoo.ShowAll;
  vao k := CloneOf(t);
  begin
    k.Name := 'Mia2';
    WriteLn(t.Name, ' ', k.Name, ' ', Name(k));
  end;
  WriteLn(p.Keys, ' ', p.Value);
end.
