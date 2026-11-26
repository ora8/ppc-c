program ArrayClone;
{* arrays with objects: assignment moves, a.Clone copies deeply *}
type
  TAnimal = class
    Name: String;
    constructor Create(AName: String const);
    destructor Destroy; override;
    function Clone: TAnimal; const; auto;
  end;
  TZoo = array[1..3] of optional TAnimal;
  TEnclosure = record                       {* array in a record: Clone is generated *}
    Animals: TZoo;
    function Clone: TEnclosure; const; auto;
  end;

constructor TAnimal.Create(AName: String const); begin inherited Create; Name := AName; end;
destructor TAnimal.Destroy; begin WriteLn('  freed ', Name); inherited Destroy; end;

procedure Show(Title: String; z: TZoo);
begin
  Write(Title, ': ');
  for val i := 1 to 3 do
    if z[i] is some t then Write(t.Name, ' ') else Write('- ');
  WriteLn;
end;

val
  a: TZoo := default;
  b: TZoo := default;
  c: TZoo := default;
  g: TEnclosure := default;
  h: TEnclosure := default;
begin
  a[1] := TAnimal.Create('Leo');
  a[2] := TAnimal.Create('Mia');

  WriteLn('1. assignment = move');
  b := a;                                {* the objects move to b, a is empty *}
  Show('a', a);
  Show('b', b);

  WriteLn('2. copy with Clone, element by element');
  for val i := 1 to 3 do
    if b[i] is some t then c[i] := t.Clone;
  if c[1] is some t then t.Name := 'Leo2';     {* changes only the copy *}
  Show('b', b);
  Show('c', c);

  WriteLn('3. copy of the whole array with a.Clone');
  val d := b.Clone;
  begin
    if d[2] is some t then t.Name := 'Mia2';
    Show('b', b);
    Show('d', d);
  end;
  WriteLn('4. copy of the whole record with Clone');
  g.Animals[1] := TAnimal.Create('Rex');
  h := g.Clone;
  Show('g', g.Animals);
  Show('h', h.Animals);
  val numbers: array[1..3] of Integer := (1, 2, 3);
  begin
    val k := numbers.Clone;                     {* without objects: simply a copy *}
    begin
      k[1] := 9;
      WriteLn('Numbers: ', numbers[1], ' ', k[1]);
    end;
  end;
  WriteLn('end of program');
end.
