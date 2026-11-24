program Strings;
{ dynamic strings: no length limit, own buffer per variable, copied on assignment }
type
  TPerson = record
    Name, City: String;
  end;
  TK = class
    Name: String;
    constructor Create(AName: String const);
    function Clone: TK; const; auto;
  end;
constructor TK.Create(AName: String const); begin inherited Create; Name := AName; end;

function Repeat_(s: String; n: Integer): String;
begin
  Result := '';
  for val i := 1 to n do Result := Result + s;
end;

function Shout(s: String): String;
begin
  s := UpperCase(s);          { changes only the copy }
  s[1] := '!';
  Result := s + '!';
end;

val
  a := 'Hello';
  b := '';
  long: String := '';
  p: TPerson := (Name: 'Anna'; City: 'Bern');
  q: TPerson := default;
  names: array of String := ('x', 'y');
  o: optional String := 'opt';
vao k := TK.Create('obj');
begin
  b := a;
  b[1] := 'J';
  WriteLn(a, ' ', b);
  long := Repeat_('abc', 200);
  WriteLn(Length(long), ' ', Copy(long, 598, 3));
  WriteLn(Shout(a), ' ', a);
  q := p;
  q.Name := 'Bert';
  WriteLn(p.Name, ' ', q.Name, ' ', q.City);
  SetLength(names, 3);
  names[2] := 'z' + IntToStr(42);
  WriteLn(names[0], names[1], names[2], ' ', Length(names));
  Delete(long, 1, 590);
  Insert('<', long, 1);
  WriteLn(long, ' ', Pos('cab', long));
  if o is some s then WriteLn(s);
  vao k2 := k.Clone;
  begin
    k2.Name[1] := 'O';
    WriteLn(k.Name, ' ', k2.Name);
  end;
  for val i := 1 to 100000 do b := a + IntToStr(i);
  WriteLn(b);
end.
