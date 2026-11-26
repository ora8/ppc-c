program Misc;
type
  TArr5 = array[1..5] of Integer;
  TDay = (Mon, Tue, Wed, Thu, Fri, Sat, Sun);
  TAccount = class
  private
    FBalance: Integer;
    FLog: String;
    procedure SetBalance(V: Integer);
  public
    property Balance: Integer read FBalance write SetBalance;
    property Log: String read FLog;
    procedure Apply(n: Integer);
  end;
  TRec = class A, B: Integer; Name: String; end;

procedure TAccount.SetBalance(V: Integer);
begin
  FLog := FLog + '[' + IntToStr(V) + ']';
  FBalance := V;
end;

procedure TAccount.Apply(n: Integer);
  procedure Step(k: Integer);          {* nested in a method, access to Self *}
  begin
    Balance := Balance + k;
  end;
val i: Integer := default;
begin
  for i := 1 to n do Step(i);
end;

function IsWeekend(d: TDay): Boolean; forward;

function Describe(d: TDay): String;
begin
  case d of
    Mon..Fri: Result := 'weekday';
  else
    Result := 'weekend';
  end;
  if IsWeekend(d) then Result := Result + '!';
end;

function IsWeekend(d: TDay): Boolean;
begin
  IsWeekend := d >= Sat;
end;

function FirstNeg(a: TArr5 const): Integer;
val i: Integer := default;
begin
  for i := 1 to 5 do
    if a[i] < 0 then Exit(i);
  Result := -1;
end;

function Outer(x: Integer): Integer;
val depth: Integer := default;
  function Mid(y: Integer): Integer;
    function Inner(z: Integer): Integer;
    begin
      depth := depth + 1;
      Inner := x + y + z;     {* reaches two levels outwards *}
    end;
  begin
    Mid := Inner(y * 2);
  end;
begin
  depth := 0;
  Result := Mid(x) * 10 + depth;
end;

vao
  acc: TAccount := TAccount.Create;
  p: TRec := TRec.Create;
val
  d: TDay := default;
  arr: TArr5 := default;
  s, t: String := default;
  i: Integer := default;
begin
  
  acc.Balance := 100;
  acc.Apply(3);
  WriteLn('Kontostand: ', acc.Balance, ' Log: ', acc.Log);

  for d := Mon to Sun do WriteLn(d, ': ', Describe(d), ' ord=', Ord(d));
  WriteLn('Succ(Mon)=', Succ(Mon), ' High=', High(TDay));

  
  p.A := 1; p.B := 2; p.Name := 'rec';
  WriteLn(p.Name, ' ', p.A + p.B, ' ', p.B);

  arr[1] := 3; arr[2] := 1; arr[3] := -4; arr[4] := 1; arr[5] := -5;
  WriteLn('first negative at index ', FirstNeg(arr));
  WriteLn('Outer(2) = ', Outer(2));

  s := 'Hello World';
  t := s;
  Delete(t, 1, 6);
  Insert('nice ', s, 7);
  WriteLn(s, ' | ', t, ' | ', UpperCase(t), ' | ', LowerCase('ABC'));
  case t of
    'Welt': WriteLn('String-case ok');
  else WriteLn('string case wrong');
  end;
  Str(3.14159:0:2, s);
  WriteLn('Str: ', s, ' Round(2.5)=', Round(2.5), ' Trunc(-2.7)=', Trunc(-2.7));
  i := StrToInt('123') + 1;
  WriteLn(i, ' ', 'x':5, '|', 42:6, '|', True:6, '|');
  WriteLn('End');
end.
