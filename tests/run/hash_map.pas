program hash_map;
import std;
type
  TItem = class N: Integer; constructor Create(AN: Integer); destructor Destroy; override; end;
  TPoint = record X, Y: Integer; end;
constructor TItem.Create(AN: Integer); begin N := AN; end;
destructor TItem.Destroy; begin if N < 3 then WriteLn('free ', N); end;
function Pt(x, y: Integer): TPoint; begin Result.X := x; Result.Y := y; end;
val total := 0;
begin
  vao ages := THashMap<String, Integer>.Create;
  vao objs := THashMap<Integer, TItem>.Create;
  vao grid := THashMap<TPoint, String>.Create;
  begin
    ages['Anna'] := 30;
    ages['Bert'] := 41;
    ages['Anna'] := 31;
    if ages['Anna'] is some a then WriteLn('Anna ', a);
    WriteLn(ages.Count, ' ', ages.Contains('Bert'), ' ', ages.Contains('Cleo'));
    if ages.Remove('Bert') is some b then WriteLn('removed ', b);
    WriteLn(ages.Count, ' ', ages['Bert'].HasValue);
    for val i := 1 to 10000 do ages.Put('k' + IntToStr(i), i);
    for val i := 1 to 10000 do
      if ages['k' + IntToStr(i)] is some v then total := total + v;
    val keys := ages.KeyList;
    begin
      WriteLn(ages.Count, ' ', total, ' ', Length(keys));
    end;
    objs.Put(1, TItem.Create(1));
    objs.Put(2, TItem.Create(2));
    objs[1] := TItem.Create(10);
    if objs[2] is some o then WriteLn('obj ', o.N);
    grid[Pt(1, 2)] := 'a';
    grid[Pt(2, 1)] := 'b';
    if grid[Pt(2, 1)] is some g then WriteLn('grid ', g);
    WriteLn(Hash(42) = Hash(42), ' ', Hash('ab') <> Hash('ba'));
  end;
  WriteLn('end');
end.
