{* EXPECT: Current of "TBagEnum" gives a copy *}
program T;
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
function TBagEnum.MoveNext: Boolean; begin Inc(I); Result := False; if Bag is some b then Result := I < Length(b.Items); end;
function TBagEnum.Current: Integer; const; begin Result := 0; if Bag is some b then Result := b.Items[I]; end;
procedure TBag.Add(x: Integer); begin SetLength(Items, Length(Items) + 1); Items[High(Items)] := x; end;
function TBag.GetEnumerator: TBagEnum; begin Result.I := -1; Result.Bag := Self; end;
begin vao bag := TBag.Create; begin for var x in bag do x := 1; end; end.
