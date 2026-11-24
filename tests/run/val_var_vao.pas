program ValVarVao;
{ Three kinds of variables:
    val  x := 5;                 value (Integer, String, record, array, optional)
    val  s := TAccount.Create;   stack object: in the stack frame, Destroy at the end of the block
    var  r: Integer := x;        reference to another variable, owns nothing
    vao  o := TAccount.Create;   owns the object: freed at the end of the scope
  'T const' makes a variable or a parameter unchangeable;
  a 'T const' parameter is passed by reference, without a copy. }
type
  TAccount = class
    Name: String;
    Balance: Integer;
    constructor Create(AName: String const; ABalance: Integer);
    destructor Destroy; override;
    class function Open(AName: String const): TAccount;   { without Self }
    procedure Deposit(Amount: Integer);
    function Info: String; const;
  end;
  TPoint = record
    X, Y: Integer;
    class function Origin: TPoint;
  end;

constructor TAccount.Create(AName: String const; ABalance: Integer);
begin inherited Create; Name := AName; Balance := ABalance; end;
destructor TAccount.Destroy; begin WriteLn('  freed: ', Name); inherited Destroy; end;
class function TAccount.Open(AName: String const): TAccount;
begin Result := TAccount.Create(AName, 10); end;              { Startguthaben }
procedure TAccount.Deposit(Amount: Integer); begin Balance := Balance + Amount; end;
function TAccount.Info: String; const; begin Result := Name + ': ' + IntToStr(Balance); end;
class function TPoint.Origin: TPoint; begin Result := (X: 0; Y: 0); end;

procedure Show(K: TAccount const);           { borrow, read-only }
begin WriteLn('  ', K.Info); end;

procedure Duplicate(var N: Integer);        { var parameter: changes the original }
begin N := N * 2; end;

procedure Local;
vao
  k := TAccount.Open('local');            { belongs to Local }
begin
  Show(k);
end;                                        { k is freed here }

procedure Fast;
val
  s := TAccount.Create('stack', 5);           { stack object: no malloc, no free }
begin
  s.Deposit(1);
  Show(s);                                 { borrow as with a heap object }
end;                                        { s.Destroy runs here }

val
  number := 21;
  p := TPoint.Origin;
  limit: Integer const := 100;              { unchangeable }
vao
  anna := TAccount.Create('Anna', 50);
  bert := TAccount.Open('Bert');
begin
  WriteLn('--- vao: owner');
  Local;
  Fast;
  anna.Deposit(25);
  Show(anna);
  Show(bert);

  WriteLn('--- var: reference without ownership');
  var
    r: Integer := number;                     { r is another name for number }
    k: TAccount := anna;                      { k borrows anna, frees nothing }
  begin
    r := r + 1;
    k.Deposit(5);
    Duplicate(r);
  end;
  WriteLn('  number = ', number, ', anna = ', anna.Balance);

  WriteLn('--- const');
  p.X := limit;
  WriteLn('  p = (', p.X, ', ', p.Y, '), limit = ', limit);
  WriteLn('--- end of program');
end.                                        { anna and bert are freed }
