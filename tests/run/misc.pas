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
    procedure Apply(n: Integer);
  end;
  TRec = class A, B: Integer; Name: String; end;

procedure TAccount.SetBalance(V: Integer);
begin
  FLog := FLog + '[' + IntToStr(V) + ']';
  FBalance := V;
end;

procedure TAccount.Apply(n: Integer);
  procedure Step(k: Integer);          { verschachtelt in Methode, Zugriff auf Self }
  begin
    Balance := Balance + k;
  end;
var i: Integer;
begin
  for i := 1 to n do Step(i);
end;

function IsWeekend(d: TDay): Boolean; forward;

function Describe(d: TDay): String;
begin
  case d of
    Mon..Fri: Result := 'Werktag';
  else
    Result := 'Wochenende';
  end;
  if IsWeekend(d) then Result := Result + '!';
end;

function IsWeekend(d: TDay): Boolean;
begin
  IsWeekend := d >= Sat;
end;

function FirstNeg(const a: TArr5): Integer;
var i: Integer;
begin
  for i := 1 to 5 do
    if a[i] < 0 then Exit(i);
  Result := -1;
end;

function Outer(x: Integer): Integer;
var depth: Integer;
  function Mid(y: Integer): Integer;
    function Inner(z: Integer): Integer;
    begin
      depth := depth + 1;
      Inner := x + y + z;     { greift zwei Ebenen nach aussen }
    end;
  begin
    Mid := Inner(y * 2);
  end;
begin
  depth := 0;
  Result := Mid(x) * 10 + depth;
end;

heap
  acc: TAccount;
  p: TRec;
var
  d: TDay;
  arr: TArr5;
  s, t: String;
  i: Integer;
begin
  acc := TAccount.Create;
  acc.Balance := 100;
  acc.Apply(3);
  WriteLn('Kontostand: ', acc.Balance, ' Log: ', acc.FLog);
  acc.Free;

  for d := Mon to Sun do WriteLn(d, ': ', Describe(d), ' ord=', Ord(d));
  WriteLn('Succ(Mon)=', Succ(Mon), ' High=', High(TDay));

  p := TRec.Create;
  with p do begin A := 1; B := 2; Name := 'rec'; end;
  WriteLn(p.Name, ' ', p.A + p.B, ' ', p.B);
  p.Free;

  arr[1] := 3; arr[2] := 1; arr[3] := -4; arr[4] := 1; arr[5] := -5;
  WriteLn('erstes negatives an Index ', FirstNeg(arr));
  WriteLn('Outer(2) = ', Outer(2));

  s := 'Hallo Welt';
  t := s;
  Delete(t, 1, 6);
  Insert('schoene ', s, 7);
  WriteLn(s, ' | ', t, ' | ', UpperCase(t), ' | ', LowerCase('ABC'));
  case t of
    'Welt': WriteLn('String-case ok');
  else WriteLn('String-case falsch');
  end;
  Str(3.14159:0:2, s);
  WriteLn('Str: ', s, ' Round(2.5)=', Round(2.5), ' Trunc(-2.7)=', Trunc(-2.7));
  i := StrToInt('123') + 1;
  WriteLn(i, ' ', 'x':5, '|', 42:6, '|', True:6, '|');
  WriteLn('Ende');
end.
