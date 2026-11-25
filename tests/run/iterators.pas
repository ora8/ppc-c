program iterators;
type
  TBag = class;
  TBagEnum = record
    Bag: var optional TBag;
    I: Integer;
    function MoveNext: Boolean;
    function Current: Integer; const;
  end;
  TBag = class
    Items: array of Integer;
    procedure Add(x: Integer);
    function GetEnumerator: TBagEnum;
  end;
  TRangeEnum = record
    Cur, Hi, Step: Integer;
    function MoveNext: Boolean;
    property Current: Integer read Cur;
  end;
  TRange = record
    Lo, Hi, Step: Integer;
    function GetEnumerator: TRangeEnum; const;
  end;
  TWordEnum = class
    Text: String;
    Pos, Start: Integer;
    W: String;
    function MoveNext: Boolean;
    function Current: String; const;
  end;
  TWords = record
    Text: String;
    function GetEnumerator: TWordEnum; const;
  end;
function TBagEnum.MoveNext: Boolean;
begin
  Inc(I);
  Result := False;
  if Bag is some b then Result := I < Length(b.Items);
end;
function TBagEnum.Current: Integer; const;
begin
  Result := 0;
  if Bag is some b then Result := b.Items[I];
end;
procedure TBag.Add(x: Integer); begin SetLength(Items, Length(Items) + 1); Items[High(Items)] := x; end;
function TBag.GetEnumerator: TBagEnum;
begin
  Result.I := -1;
  Result.Bag := Self;
end;
function TRangeEnum.MoveNext: Boolean; begin Cur := Cur + Step; Result := Cur <= Hi; end;
function TRange.GetEnumerator: TRangeEnum; const; begin Result.Cur := Lo - Step; Result.Hi := Hi; Result.Step := Step; end;
function Range(lo, hi, step: Integer): TRange; begin Result.Lo := lo; Result.Hi := hi; Result.Step := step; end;
function TWordEnum.MoveNext: Boolean;
begin
  while (Pos <= Length(Text)) and (Text[Pos] = ' ') do Inc(Pos);
  Start := Pos;
  while (Pos <= Length(Text)) and (Text[Pos] <> ' ') do Inc(Pos);
  W := Copy(Text, Start, Pos - Start);
  Result := Pos > Start;
end;
function TWordEnum.Current: String; const; begin Result := W; end;
function TWords.GetEnumerator: TWordEnum; const;
begin
  Result := TWordEnum.Create;
  Result.Text := Text;
  Result.Pos := 1;
end;
val
  w: TWords := (Text: '  the quick  brown fox ');
begin
  vao bag := TBag.Create;
  begin
    bag.Add(3); bag.Add(5); bag.Add(8);
    for val x in bag do Write(x, ' ');
    WriteLn;
  end;
  for val i in Range(1, 10, 3) do Write(i, ' ');
  WriteLn;
  for val word in w do Write('[', word, ']');
  WriteLn;
end.
