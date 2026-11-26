program generic_borrow_ok;
{* generic bodies that are correct for objects and for values *}
type
  TItem = class
    N: Integer;
    constructor Create(AN: Integer);
  end;
  TPair<T> = record
    A, B: optional T;
  end;
constructor TItem.Create(AN: Integer); begin N := AN; end;

procedure Use<T>(vao x: T); begin end;

function Both<T>(vao x, y: T): TPair<T>;
begin
  Result.A := x;
  Result.B := y;
end;

procedure SwapFirst<T>(var a: array of optional T);
val h: optional T := none;
begin
  if Length(a) < 2 then Exit;
  h := Take(a[0]);
  a[0] := Take(a[1]);
  a[1] := h;
end;

function Twice<T: Copy>(x: T): TPair<T>;
begin
  Result.A := x;
  Result.B := x;
end;

val
  items: array of optional TItem := default;
  nums: array of optional Integer := default;
begin
  SetLength(items, 2);
  items[0] := TItem.Create(1);
  items[1] := TItem.Create(2);
  SwapFirst(items);
  if items[0] is some k then WriteLn(k.N);
  SetLength(nums, 2);
  nums[0] := 10;
  nums[1] := 20;
  SwapFirst(nums);
  if nums[0] is some n then WriteLn(n);
  val p := Both(TItem.Create(3), TItem.Create(4));
  begin
    if p.B is some b then WriteLn(b.N);
  end;
  val q := Twice(7);
  begin
    if q.A is some a then WriteLn(a);
  end;
end.
