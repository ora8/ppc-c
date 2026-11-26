program OwnTraits;
{* own types become Hashable, Equatable and Comparable with Hash, Equals and CompareTo (records and classes) *}
import std;
type
  TPoint = record
    X, Y: Integer;
    function Hash: Integer; const;
    function Equals(other: TPoint): Boolean; const;
  end;
  TVersion = record
    Major, Minor: Integer;
    function CompareTo(other: TVersion): Integer; const;
  end;
  TMoney = class
    Cents: Int64;
    constructor Create(ACents: Int64);
    function Equals(other: TMoney): Boolean; const;
    function CompareTo(other: TMoney): Integer; const;
  end;

function TPoint.Hash: Integer; const; begin Result := X * 31 + Y; end;
function TPoint.Equals(other: TPoint): Boolean; const; begin Result := (X = other.X) and (Y = other.Y); end;
function TVersion.CompareTo(other: TVersion): Integer; const;
begin
  if Major <> other.Major then Result := Major - other.Major else Result := Minor - other.Minor;
end;
constructor TMoney.Create(ACents: Int64); begin inherited Create; Cents := ACents; end;
function TMoney.Equals(other: TMoney): Boolean; const; begin Result := Cents = other.Cents; end;
function TMoney.CompareTo(other: TMoney): Integer; const;
begin
  if Cents < other.Cents then Result := -1 else if Cents > other.Cents then Result := 1 else Result := 0;
end;

function Smaller<T: Comparable>(a, b: T): Boolean;
begin
  Result := a < b;
end;

vao names := THashMap<TPoint, String>.Create;
val vs: array of TVersion := ((Major: 2; Minor: 1), (Major: 1; Minor: 9), (Major: 2; Minor: 0));
val p: TPoint := (X: 1; Y: 2);
val q: TPoint := (X: 1; Y: 2);
vao a := TMoney.Create(500);
vao b := TMoney.Create(500);
vao c := TMoney.Create(700);
begin
  names.Put(p, 'home');
  names.Put((X: 3; Y: 4), 'work');
  if names.Get(q) is some n then WriteLn('(1,2) is ', n);       {* a different but equal key *}
  WriteLn('count ', names.Count, ', equal points: ', p = q, ', hash ', Hash(p));
  Sort(vs);
  for val v in vs do Write(v.Major, '.', v.Minor, ' ');
  WriteLn;
  WriteLn('newest: ', Max(vs[0], vs[2]).Major, '.', Max(vs[0], vs[2]).Minor);
  WriteLn('money equal: ', a = b, ', different: ', a <> c, ', smaller: ', a < c, ' ', Smaller(c, a));
end.
