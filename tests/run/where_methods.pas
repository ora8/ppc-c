program WhereMethods;
{* methods of generic types with an extra condition: 'where T: Comparable;' - they exist only for
   instances whose type argument satisfies it (TList<T>.Sort in std is one) *}
import std;
type
  TPair<T> = class
    A, B: T;
    constructor Create(vao AA, AB: T);
    function Larger: T; where T: Comparable;
    function Same: Boolean; where T: Equatable;
    procedure Swap; where T: Copy;
  end;
  TPerson = class
    Name: String;
    constructor Create(AName: String);
  end;

constructor TPair<T>.Create(vao AA, AB: T);
begin
  inherited Create;
  A := AA;
  B := AB;
end;
function TPair<T>.Larger: T;
begin
  if A < B then Result := B else Result := A;
end;
function TPair<T>.Same: Boolean;
begin
  Result := A = B;
end;
procedure TPair<T>.Swap;
val h: T := default;
begin
  h := A;
  A := B;
  B := h;
end;
constructor TPerson.Create(AName: String);
begin
  inherited Create;
  Name := AName;
end;

vao ints := TPair<Integer>.Create(3, 7);
vao words := TPair<String>.Create('pear', 'apple');
vao nums := TList<Real>.Create;
begin
  WriteLn(ints.Larger, ' ', ints.Same, ' ', words.Larger, ' ', words.Same);
  ints.Swap;
  WriteLn(ints.A, ' ', ints.B);
  nums.Add(2.5); nums.Add(-1.0); nums.Add(0.5);
  nums.Sort;
  for val x in nums do Write(x:0:1, ' ');
  WriteLn;
end.
