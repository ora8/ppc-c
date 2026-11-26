program ListSort;
{* sorting lists: SortList (comparable elements) and the method SortBy (any elements, also objects; stable) *}
import std;
type
  TPerson = class
    Name: String;
    Age: Integer;
    constructor Create(AName: String; AAge: Integer);
  end;
constructor TPerson.Create(AName: String; AAge: Integer);
begin
  inherited Create;
  Name := AName;
  Age := AAge;
end;

vao nums := TList<Integer>.Create;
vao words := TList<String>.Create;
vao people := TList<TPerson>.Create;
begin
  nums.Add(5); nums.Add(2); nums.Add(9); nums.Add(1); nums.Add(5);
  SortList(nums);
  for val n in nums do Write(n, ' ');
  WriteLn;
  nums.SortBy(function(x, y: Integer const): Boolean begin Result := x > y; end);   {* descending *}
  for val n in nums do Write(n, ' ');
  WriteLn;
  words.Add('pear'); words.Add('apple'); words.Add('fig'); words.Add('kiwi');
  SortList(words);
  for val w in words do Write(w, ' ');
  WriteLn;
  words.SortBy(function(x, y: String const): Boolean begin Result := Length(x) < Length(y); end);   {* stable: kiwi stays before pear *}
  for val w in words do Write(w, ' ');
  WriteLn;
  if words.Extract(0) is some w then WriteLn('extracted ', w, ', ', words.Count, ' left');
  people.Add(TPerson.Create('Carl', 40));
  people.Add(TPerson.Create('Anna', 30));
  people.Add(TPerson.Create('Bert', 30));
  people.SortBy(function(x, y: TPerson const): Boolean begin Result := x.Age < y.Age; end);
  for val p in people do Write(p.Name, ' ', p.Age, ', ');
  WriteLn;
end.
