{ ppc_std: the standard library of Perfect Pascal  (include it with 'import ppc_std;')

  TList<T>                  list that grows (index from 0)
  TStack<T>                 stack (last in, first out)
  TMap<K: Comparable; V>    map key -> value, sorted by key
  Sort<T: Comparable>       sorts an 'array of T' in ascending order
  IndexOf<T: Equatable>     position of a value in an 'array of T', or none

  SortRange is private to the implementation.

  All containers own their elements (objects are freed with the container).
  Taking elements out (Extract, Pop, Remove) passes the ownership to the caller. }

module ppc_std;

interface

type
  TList<T> = class
    Items: array of optional T;
    procedure Add(vao x: T);                       { append, takes over x }
    function Count: Integer; const;
    function Get(i: Integer): var optional T;      { element i (objects: borrowed) }
    function Extract(i: Integer): optional T;      { take element i out }
    procedure Clear;                               { free all elements }
  end;

  TStack<T> = class
    Items: array of optional T;
    procedure Push(vao x: T);
    function Pop: optional T;                      { take the top element out }
    function Peek: var optional T;                 { look at the top element }
    function Count: Integer; const;
    function IsEmpty: Boolean; const;
  end;

  TMap<K: Comparable; V> = class
    Keys: array of K;                              { sorted ascending }
    Values: array of optional V;
    function Find(k: K): Integer; const;           { first position with Keys[i] >= k }
    procedure Put(k: K; vao v: V);                 { insert or replace }
    function Get(k: K): var optional V;
    function Contains(k: K): Boolean; const;
    function Remove(k: K): optional V;             { take out }
    function Count: Integer; const;
  end;

procedure Sort<T: Comparable>(var a: array of T);
function IndexOf<T: Equatable>(a: array of T; x: T): optional Integer;

implementation

{ ------------------------------------------------------------------- TList<T> }

procedure TList<T>.Add(vao x: T);
begin
  SetLength(Items, Length(Items) + 1);
  Items[High(Items)] := x;
end;

function TList<T>.Count: Integer; const;
begin
  Result := Length(Items);
end;
function TList<T>.Get(i: Integer): var optional T;
begin
  Result := Items[i];
end;

function TList<T>.Extract(i: Integer): optional T;
begin
  Result := Take(Items[i]);
  for val j := i to High(Items) - 1 do Items[j] := Take(Items[j + 1]);
  SetLength(Items, Length(Items) - 1);
end;

procedure TList<T>.Clear;
begin
  SetLength(Items, 0);
end;

{ ------------------------------------------------------------------ TStack<T> }

procedure TStack<T>.Push(vao x: T);
begin
  SetLength(Items, Length(Items) + 1);
  Items[High(Items)] := x;
end;

function TStack<T>.Pop: optional T;
begin
  Result := none;
  if Length(Items) > 0 then
  begin
    Result := Take(Items[High(Items)]);
    SetLength(Items, Length(Items) - 1);
  end;
end;

function TStack<T>.Peek: var optional T;
begin
  Result := none;
  if Length(Items) > 0 then Result := Items[High(Items)];
end;

function TStack<T>.Count: Integer; const;
begin
  Result := Length(Items);
end;

function TStack<T>.IsEmpty: Boolean; const;
begin
  Result := Length(Items) = 0;
end;

{ ------------------------------------------------------------------ TMap<K, V> }

function TMap<K, V>.Find(k: K): Integer; const;
val
  lo := 0;
  hi := Length(Keys);
begin
  while lo < hi do                                 { binary search }
    val m := (lo + hi) div 2;
    begin
      if Keys[m] < k then lo := m + 1 else hi := m;
    end;
  Result := lo;
end;

procedure TMap<K, V>.Put(k: K; vao v: V);
val i := Find(k);
begin
  if (i < Length(Keys)) and (Keys[i] = k) then
    Values[i] := v                                 { replace: the old value is freed }
  else
  begin
    SetLength(Keys, Length(Keys) + 1);
    SetLength(Values, Length(Values) + 1);
    for val j := High(Keys) downto i + 1 do
    begin
      Keys[j] := Keys[j - 1];
      Values[j] := Take(Values[j - 1]);
    end;
    Keys[i] := k;
    Values[i] := v;
  end;
end;

function TMap<K, V>.Get(k: K): var optional V;
val i := Find(k);
begin
  Result := none;
  if (i < Length(Keys)) and (Keys[i] = k) then Result := Values[i];
end;

function TMap<K, V>.Contains(k: K): Boolean; const;
val i := Find(k);
begin
  Result := (i < Length(Keys)) and (Keys[i] = k);
end;

function TMap<K, V>.Remove(k: K): optional V;
val i := Find(k);
begin
  Result := none;
  if (i < Length(Keys)) and (Keys[i] = k) then
  begin
    Result := Take(Values[i]);
    for val j := i to High(Keys) - 1 do
    begin
      Keys[j] := Keys[j + 1];
      Values[j] := Take(Values[j + 1]);
    end;
    SetLength(Keys, Length(Keys) - 1);
    SetLength(Values, Length(Values) - 1);
  end;
end;

function TMap<K, V>.Count: Integer; const;
begin
  Result := Length(Keys);
end;

{ ------------------------------------------------------------- sorting, searching }

procedure SortRange<T: Comparable>(var a: array of T; lo, hi: Integer);
val
  i := lo;
  j := hi;
begin
  if lo >= hi then Exit;
  val pivot: T := a[(lo + hi) div 2];
  begin
    while i <= j do                                { quicksort }
    begin
      while a[i] < pivot do Inc(i);
      while pivot < a[j] do Dec(j);
      if i <= j then
      begin
        val h: T := a[i];
        begin
          a[i] := a[j];
          a[j] := h;
        end;
        Inc(i);
        Dec(j);
      end;
    end;
  end;
  SortRange(a, lo, j);
  SortRange(a, i, hi);
end;

procedure Sort<T: Comparable>(var a: array of T);
begin
  SortRange(a, 0, High(a));
end;

function IndexOf<T: Equatable>(a: array of T; x: T): optional Integer;
begin
  Result := none;
  for val i := 0 to High(a) do
    if (a[i] = x) and not Result.HasValue then Result := i;
end;

end.
