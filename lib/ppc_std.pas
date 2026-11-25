{ ppc_std: the standard library of Perfect Pascal  (include it with 'import ppc_std;')

  TList<T>                  list that grows (index from 0)
  TStack<T>                 stack (last in, first out)
  TMap<K: Comparable; V>    map key -> value, sorted by key
  THashMap<K: Hashable; V>  map key -> value with hashing (faster for many keys)
  Sort<T: Comparable>       sorts an 'array of T' in ascending order
  SortBy<T: Copy>           sorts with a comparison function: SortBy(a, function(x, y: T): Boolean ...)
  IndexOf<T: Equatable>     position of a value in an 'array of T', or none
  Filter<T: Copy>           the elements for which a function returns True

  Strings     Trim, TrimLeft, TrimRight, StartsWith, EndsWith, Contains, PosFrom, ReplaceStr,
              Split, Join, SplitLines, ReverseString, DupeString, PadLeft, PadRight, IntToHex,
              TryStrToInt, TryStrToFloat, StrToFloat, IsDigit, IsLetter, IsSpace
  Math        Pi, Min, Max, EnsureRange, Floor, Ceil, Power, Tan, ArcSin, ArcCos, ArcTan2, Log10,
              Log2, Hypot, Sign, IsNan
  Files       ReadFile, ReadLines, WriteFile, AppendFile, FileExists, DeleteFile,
              TTextFile (OpenRead, OpenWrite, OpenAppend: result TTextFile)
  Time        MilliSeconds, UnixTime, Sleep, Now (TDateTime), DateToStr, TimeToStr, DateTimeToStr
  Program     ParamCount, ParamStr, GetEnv

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
    procedure Put(i: Integer; vao x: T);           { replace element i (the old one is freed) }
    function Extract(i: Integer): optional T;      { take element i out }
    procedure Clear;                               { free all elements }
    property Item[i: Integer]: optional T read Get write Put; default; { l[i], l[i] := x }
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
    property Value[k: K]: optional V read Get write Put; default; { m[k], m[k] := v }
  end;

  THashMap<K: Hashable; V> = class                 { map key -> value with hashing: Put/Get/Remove in O(1) }
  private
    FKeys: array of K;
    FValues: array of optional V;
    FState: array of Integer;                      { per slot: 0 empty, 1 used, 2 removed }
    FCount, FUsed: Integer;                        { FUsed: used and removed slots }
    function Slot(k: K): Integer; const;           { the slot of k, -1 if k is not there }
    procedure Grow;
  public
    procedure Put(k: K; vao v: V);                 { insert or replace (the old value is freed) }
    function Get(k: K): var optional V;
    function Contains(k: K): Boolean; const;
    function Remove(k: K): optional V;             { take out }
    function Count: Integer; const;
    function KeyList: array of K; const;           { the keys (in no particular order) }
    procedure Clear;
    property Value[k: K]: optional V read Get write Put; default; { m[k], m[k] := v }
  end;

procedure Sort<T: Comparable>(var a: array of T);
procedure SortBy<T: Copy>(var a: array of T; Less: reference to function(x, y: T): Boolean);
function IndexOf<T: Equatable>(a: array of T; x: T): optional Integer;
function Filter<T: Copy>(a: array of T; Keep: reference to function(x: T): Boolean): array of T;
{ ---- strings ---- }
function Trim(s: String): String;                  { without spaces/control characters at both ends }
function TrimLeft(s: String): String;
function TrimRight(s: String): String;
function StartsWith(s, Prefix: String): Boolean;
function EndsWith(s, Suffix: String): Boolean;
function Contains(s, Sub: String): Boolean;
function PosFrom(Sub, s: String; From: Integer): Integer; external '__std_posfrom'; { first position >= From, 0 if none }
function ReplaceStr(s, Old, New: String): String;  { every occurrence }
function Split(s, Sep: String): array of String;    { Split('a,b,,c', ',') = ('a', 'b', '', 'c') }
function Join(Parts: array of String; Sep: String): String;
function SplitLines(s: String): array of String;    { at LF or CR LF; no empty last line }
function ReverseString(s: String): String;
function DupeString(s: String; Count: Integer): String;
function PadLeft(s: String; Width: Integer; Fill: Char): String;
function PadRight(s: String; Width: Integer; Fill: Char): String;
function IntToHex(Value: Int64; Digits: Integer): String; external '__std_inttohex';
function TryStrToInt(s: String): optional Int64;    { none if s is not a whole number }
function TryStrToFloat(s: String): optional Real;
function StrToFloat(s: String): Real;               { run-time error 106 if s is not a number }
function IsDigit(c: Char): Boolean;
function IsLetter(c: Char): Boolean;
function IsSpace(c: Char): Boolean;

{ ---- math ---- }
const Pi = 3.14159265358979323846;
function Min<T: Comparable>(a, b: T): T;
function Max<T: Comparable>(a, b: T): T;
function EnsureRange<T: Comparable>(x, Lo, Hi: T): T;
function Floor(x: Real): Int64; external '__std_floor';
function Ceil(x: Real): Int64; external '__std_ceil';
function Power(x, y: Real): Real; external '__std_power';
function Tan(x: Real): Real; external '__std_tan';
function ArcSin(x: Real): Real; external '__std_arcsin';
function ArcCos(x: Real): Real; external '__std_arccos';
function ArcTan2(y, x: Real): Real; external '__std_arctan2';
function Log10(x: Real): Real; external '__std_log10';
function Log2(x: Real): Real; external '__std_log2';
function Hypot(x, y: Real): Real; external '__std_hypot';
function IsNan(x: Real): Boolean; external '__std_isnan';
function Sign(x: Real): Integer;                    { -1, 0 or 1 }

{ ---- files (whole files; errors as results) ---- }
function ReadFile(Name: String): result String;
function ReadLines(Name: String): result array of String;
function WriteFile(Name, Text: String): result;
function AppendFile(Name, Text: String): result;
function DeleteFile(Name: String): result;
function FileExists(Name: String): Boolean; external '__std_fileexists';

type
  TTextFile = class                                { a text file read or written line by line }
  private
    FHandle: Int64;
    FName: String;
    class function Open(Name, Mode: String): result TTextFile;
  public
    class function OpenRead(Name: String): result TTextFile;
    class function OpenWrite(Name: String): result TTextFile;   { creates or empties the file }
    class function OpenAppend(Name: String): result TTextFile;
    function ReadLine: optional String;            { the next line, none at the end }
    function EndOfFile: Boolean; const;
    procedure Write(s: String);
    procedure WriteLine(s: String);
    function Name: String; const;
    destructor Destroy; override;                  { closes the file }
  end;

{ ---- time ---- }
type
  TDateTime = record
    Year, Month, Day, Hour, Minute, Second, MilliSecond: Integer;
  end;
function MilliSeconds: Int64; external '__std_milliseconds'; { monotonic clock, for measuring }
function UnixTime: Int64; external '__std_unixtime';         { seconds since 1970-01-01 UTC }
procedure Sleep(ms: Integer); external '__std_sleep';
function Now: TDateTime;                                      { local time }
function DateToStr(t: TDateTime): String;                     { 2026-11-25 }
function TimeToStr(t: TDateTime): String;                     { 14:05:09 }
function DateTimeToStr(t: TDateTime): String;                 { 2026-11-25 14:05:09 }

{ ---- program ---- }
function ParamCount: Integer; external '__std_paramcount';
function ParamStr(i: Integer): String; external '__std_paramstr'; { 0: the program }
function GetEnv(Name: String): String; external '__std_getenv';   { '' if not set }

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

procedure TList<T>.Put(i: Integer; vao x: T);
begin
  Items[i] := x;
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

{ ---------------------------------------------------------------- THashMap<K, V> }

function THashMap<K, V>.Slot(k: K): Integer; const;
val
  n := Length(FState);
  i := 0;
  probes := 0;
begin
  Result := -1;
  if n = 0 then Exit;
  i := Hash(k) and (n - 1);
  while (FState[i] <> 0) and (probes < n) do
  begin
    if (FState[i] = 1) and (FKeys[i] = k) then Exit(i);
    i := (i + 1) and (n - 1);
    Inc(probes);
  end;
end;

procedure THashMap<K, V>.Grow;
val
  oldKeys := FKeys;                                { moved out: the fields are empty now }
  oldValues := FValues;
  oldState := FState;
  n := Length(oldState) * 2;
begin
  if n < 8 then n := 8;
  SetLength(FKeys, n);
  SetLength(FValues, n);
  SetLength(FState, n);
  FCount := 0;
  FUsed := 0;
  for val i := 0 to High(oldState) do
    if oldState[i] = 1 then
      if Take(oldValues[i]) is some v then Put(oldKeys[i], v);
end;

procedure THashMap<K, V>.Put(k: K; vao v: V);
val i := Slot(k);
begin
  if i >= 0 then
  begin
    FValues[i] := v;                               { replace: the old value is freed }
    Exit;
  end;
  if (FUsed + 1) * 10 >= Length(FState) * 7 then Grow;
  i := Hash(k) and (Length(FState) - 1);
  while FState[i] = 1 do i := (i + 1) and (Length(FState) - 1);
  if FState[i] = 0 then Inc(FUsed);
  FState[i] := 1;
  FKeys[i] := k;
  FValues[i] := v;
  Inc(FCount);
end;

function THashMap<K, V>.Get(k: K): var optional V;
val i := Slot(k);
begin
  Result := none;
  if i >= 0 then Result := FValues[i];
end;

function THashMap<K, V>.Contains(k: K): Boolean; const;
begin
  Result := Slot(k) >= 0;
end;

function THashMap<K, V>.Remove(k: K): optional V;
val i := Slot(k);
begin
  Result := none;
  if i >= 0 then
  begin
    Result := Take(FValues[i]);
    FState[i] := 2;                                { removed: later keys of the probe chain stay reachable }
    Dec(FCount);
  end;
end;

function THashMap<K, V>.Count: Integer; const;
begin
  Result := FCount;
end;

function THashMap<K, V>.KeyList: array of K; const;
begin
  for val i := 0 to High(FState) do
    if FState[i] = 1 then
    begin
      SetLength(Result, Length(Result) + 1);
      Result[High(Result)] := FKeys[i];
    end;
end;

procedure THashMap<K, V>.Clear;
begin
  SetLength(FKeys, 0);
  SetLength(FValues, 0);
  SetLength(FState, 0);
  FCount := 0;
  FUsed := 0;
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

procedure SortBy<T: Copy>(var a: array of T; Less: reference to function(x, y: T): Boolean);
begin                                              { insertion sort: stable }
  for val i := 1 to High(a) do
    val j := i;
    begin
      while (j > 0) and Less(a[j], a[j - 1]) do
      begin
        val h: T := a[j];
        begin
          a[j] := a[j - 1];
          a[j - 1] := h;
        end;
        Dec(j);
      end;
    end;
end;

function Filter<T: Copy>(a: array of T; Keep: reference to function(x: T): Boolean): array of T;
begin
  for val i := 0 to High(a) do
    if Keep(a[i]) then
    begin
      SetLength(Result, Length(Result) + 1);
      Result[High(Result)] := a[i];
    end;
end;

function IndexOf<T: Equatable>(a: array of T; x: T): optional Integer;
begin
  Result := none;
  for val i := 0 to High(a) do
    if (a[i] = x) and not Result.HasValue then Result := i;
end;

{ ------------------------------------------------------------------- strings }

function TrimLeft(s: String): String;
val i := 1;
begin
  while (i <= Length(s)) and (s[i] <= ' ') do Inc(i);
  Result := Copy(s, i, Length(s) - i + 1);
end;

function TrimRight(s: String): String;
val n := Length(s);
begin
  while (n > 0) and (s[n] <= ' ') do Dec(n);
  Result := Copy(s, 1, n);
end;

function Trim(s: String): String;
begin
  Result := TrimLeft(TrimRight(s));
end;

function StartsWith(s, Prefix: String): Boolean;
begin
  Result := (Length(Prefix) <= Length(s)) and (Copy(s, 1, Length(Prefix)) = Prefix);
end;

function EndsWith(s, Suffix: String): Boolean;
begin
  Result := (Length(Suffix) <= Length(s)) and (Copy(s, Length(s) - Length(Suffix) + 1, Length(Suffix)) = Suffix);
end;

function Contains(s, Sub: String): Boolean;
begin
  Result := PosFrom(Sub, s, 1) > 0;
end;

function ReplaceStr(s, Old, New: String): String;
val start := 1;
begin
  Result := '';
  if Old = '' then Exit(s);
  loop
    val p := PosFrom(Old, s, start);
    begin
      if p = 0 then Break;
      Result := Result + Copy(s, start, p - start) + New;
      start := p + Length(Old);
    end;
  Result := Result + Copy(s, start, Length(s) - start + 1);
end;

function Split(s, Sep: String): array of String;
val start := 1;
begin
  if Sep = '' then
  begin
    SetLength(Result, 1);
    Result[0] := s;
    Exit;
  end;
  loop
    val p := PosFrom(Sep, s, start);
    begin
      SetLength(Result, Length(Result) + 1);
      if p = 0 then
      begin
        Result[High(Result)] := Copy(s, start, Length(s) - start + 1);
        Break;
      end;
      Result[High(Result)] := Copy(s, start, p - start);
      start := p + Length(Sep);
    end;
end;

function Join(Parts: array of String; Sep: String): String;
begin
  Result := '';
  for val i := 0 to High(Parts) do
    if i = 0 then Result := Parts[i] else Result := Result + Sep + Parts[i];
end;

function SplitLines(s: String): array of String;
val start := 1;
begin
  for val i := 1 to Length(s) do
    if s[i] = #10 then
      val e := i - 1;
      begin
        if (e >= start) and (s[e] = #13) then Dec(e);
        SetLength(Result, Length(Result) + 1);
        Result[High(Result)] := Copy(s, start, e - start + 1);
        start := i + 1;
      end;
  if start <= Length(s) then
  begin
    SetLength(Result, Length(Result) + 1);
    Result[High(Result)] := Copy(s, start, Length(s) - start + 1);
  end;
end;

function ReverseString(s: String): String;
begin
  Result := s;
  for val i := 1 to Length(s) do Result[i] := s[Length(s) - i + 1];
end;

function DupeString(s: String; Count: Integer): String;
begin
  Result := '';
  for val i := 1 to Count do Result := Result + s;
end;

function PadLeft(s: String; Width: Integer; Fill: Char): String;
begin
  Result := s;
  while Length(Result) < Width do Result := Fill + Result;
end;

function PadRight(s: String; Width: Integer; Fill: Char): String;
begin
  Result := s;
  while Length(Result) < Width do Result := Result + Fill;
end;

function __TryStrToInt(s: String; var v: Int64): Boolean; external '__std_trystrtoint';
function __TryStrToFloat(s: String; var v: Real): Boolean; external '__std_trystrtofloat';

function TryStrToInt(s: String): optional Int64;
val v: Int64 := 0;
begin
  Result := none;
  if __TryStrToInt(s, v) then Result := v;
end;

function TryStrToFloat(s: String): optional Real;
val v: Real := 0;
begin
  Result := none;
  if __TryStrToFloat(s, v) then Result := v;
end;

function StrToFloat(s: String): Real;
val v: Real := 0;
begin
  if not __TryStrToFloat(s, v) then
  begin
    WriteLn('Runtime error 106: invalid number ''', s, '''');
    Halt(106);
  end;
  Result := v;
end;

function IsDigit(c: Char): Boolean;
begin
  Result := (c >= '0') and (c <= '9');
end;

function IsLetter(c: Char): Boolean;
begin
  Result := ((c >= 'a') and (c <= 'z')) or ((c >= 'A') and (c <= 'Z'));
end;

function IsSpace(c: Char): Boolean;
begin
  Result := (c = ' ') or (c = #9) or (c = #10) or (c = #13);
end;

{ ---------------------------------------------------------------------- math }

function Min<T: Comparable>(a, b: T): T;
begin
  if b < a then Result := b else Result := a;
end;

function Max<T: Comparable>(a, b: T): T;
begin
  if a < b then Result := b else Result := a;
end;

function EnsureRange<T: Comparable>(x, Lo, Hi: T): T;
begin
  if x < Lo then Result := Lo
  else if Hi < x then Result := Hi
  else Result := x;
end;

function Sign(x: Real): Integer;
begin
  if x > 0 then Result := 1 else if x < 0 then Result := -1 else Result := 0;
end;

{ --------------------------------------------------------------------- files }

function __ReadFile(Name: String): String; external '__std_readfile';
function __WriteFile(Name, Text: String; Append: Boolean): Boolean; external '__std_writefile';
function __DeleteFile(Name: String): Boolean; external '__std_deletefile';
function __IOError: String; external '__std_ioerror';

function ReadFile(Name: String): result String;
val t := __ReadFile(Name);
begin
  if __IOError <> '' then Result := error(__IOError) else Result := ok(t);
end;

function ReadLines(Name: String): result array of String;
val
  t := __ReadFile(Name);
  ls := SplitLines(t);
begin
  if __IOError <> '' then Result := error(__IOError) else Result := ok(ls);
end;

function WriteFile(Name, Text: String): result;
begin
  if __WriteFile(Name, Text, False) then Result := ok else Result := error(__IOError);
end;

function AppendFile(Name, Text: String): result;
begin
  if __WriteFile(Name, Text, True) then Result := ok else Result := error(__IOError);
end;

function DeleteFile(Name: String): result;
begin
  if __DeleteFile(Name) then Result := ok else Result := error(__IOError);
end;

{ ----------------------------------------------------------------- TTextFile }

function __FOpen(Name, Mode: String): Int64; external '__std_fopen';
function __FReadLine(h: Int64; var Line: String): Boolean; external '__std_freadline';
function __FWrite(h: Int64; s: String): Boolean; external '__std_fwrite';
function __FEof(h: Int64): Boolean; external '__std_feof';
procedure __FClose(h: Int64); external '__std_fclose';

class function TTextFile.Open(Name, Mode: String): result TTextFile;
val h := __FOpen(Name, Mode);
begin
  if h = 0 then Result := error(__IOError)
  else
  begin
    vao f := TTextFile.Create;
    begin
      f.FHandle := h;
      f.FName := Name;
      Result := ok(f);
    end;
  end;
end;

class function TTextFile.OpenRead(Name: String): result TTextFile;
begin
  Result := Open(Name, 'rb');
end;

class function TTextFile.OpenWrite(Name: String): result TTextFile;
begin
  Result := Open(Name, 'wb');
end;

class function TTextFile.OpenAppend(Name: String): result TTextFile;
begin
  Result := Open(Name, 'ab');
end;

function TTextFile.ReadLine: optional String;
val line := '';
begin
  Result := none;
  if __FReadLine(FHandle, line) then Result := line;
end;

function TTextFile.EndOfFile: Boolean; const;
begin
  Result := __FEof(FHandle);
end;

procedure TTextFile.Write(s: String);
begin
  __FWrite(FHandle, s);
end;

procedure TTextFile.WriteLine(s: String);
begin
  __FWrite(FHandle, s + #10);
end;

function TTextFile.Name: String; const;
begin
  Result := FName;
end;

destructor TTextFile.Destroy;
begin
  __FClose(FHandle);
  FHandle := 0;
end;

{ ---------------------------------------------------------------------- time }

procedure __Now(var y, mo, d, h, mi, s, ms: Integer); external '__std_now';

function Now: TDateTime;
begin
  Result := default;
  __Now(Result.Year, Result.Month, Result.Day, Result.Hour, Result.Minute, Result.Second, Result.MilliSecond);
end;

function DateToStr(t: TDateTime): String;
begin
  Result := PadLeft(IntToStr(t.Year), 4, '0') + '-' + PadLeft(IntToStr(t.Month), 2, '0') + '-' + PadLeft(IntToStr(t.Day), 2, '0');
end;

function TimeToStr(t: TDateTime): String;
begin
  Result := PadLeft(IntToStr(t.Hour), 2, '0') + ':' + PadLeft(IntToStr(t.Minute), 2, '0') + ':' + PadLeft(IntToStr(t.Second), 2, '0');
end;

function DateTimeToStr(t: TDateTime): String;
begin
  Result := DateToStr(t) + ' ' + TimeToStr(t);
end;

end.
