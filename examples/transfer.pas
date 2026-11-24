program Transfer;
{ Error handling without exceptions: 'result T of E' (like Result<T, E> in Rust),
  pass errors on with 'try' (like '?'), unpack with case / is ok / ValueOr.
  Compile with:  ./ppc examples/transfer.pas }
type
  TError = (NoMoney, UnknownAccount, InvalidAmount);
  TAccount = class
    Name: String;
    Balance: Integer;
    constructor Create(AName: String const; ABalance: Integer);
  end;
  TBank = class
    Accounts: array[1..3] of optional TAccount;
    function Find(Name: String const): result Integer of TError;
    function Withdraw(Nr, Amount: Integer): result of TError;
    function Transfer(Source, Dest: String const; Amount: Integer): result Integer of TError;
  end;

constructor TAccount.Create(AName: String const; ABalance: Integer);
begin inherited Create; Name := AName; Balance := ABalance; end;

function TBank.Find(Name: String const): result Integer of TError;
begin
  Result := error(UnknownAccount);
  for val i := 1 to 3 do
    if Accounts[i] is some k then
      if k.Name = Name then Result := i;       { a value is ok(i) automatically }
end;

function TBank.Withdraw(Nr, Amount: Integer): result of TError;
begin
  Result := ok;
  if Amount <= 0 then Result := error(InvalidAmount)
  else if Accounts[Nr] is some k then
    if k.Balance < Amount then Result := error(NoMoney)
    else k.Balance := k.Balance - Amount;
end;

function TBank.Transfer(Source, Dest: String const; Amount: Integer): result Integer of TError;
begin
  val a := try Find(Source);                     { error: Transfer ends here }
  val b := try Find(Dest);
  begin
    try Withdraw(a, Amount);                    { statement: only the error counts }
    if Accounts[b] is some k then
    begin
      k.Balance := k.Balance + Amount;
      Result := k.Balance;                       { new balance of the receiver }
    end;
  end;
end;

function Text(f: TError): String;
begin
  case f of
    NoMoney: Result := 'not enough money';
    UnknownAccount: Result := 'unknown account';
    InvalidAmount: Result := 'invalid amount';
  end;
end;

procedure Attempt(Bank: TBank; Source, Dest: String const; Amount: Integer);
begin
  Write(Source, ' -> ', Dest, ' ', Amount, ': ');
  case Bank.Transfer(Source, Dest, Amount) of
    ok balance: WriteLn('ok, ', Dest, ' now has ', balance);
    error f:  WriteLn('Error: ', Text(f));
  end;
end;

vao bank := TBank.Create;
begin
  bank.Accounts[1] := TAccount.Create('Anna', 100);
  bank.Accounts[2] := TAccount.Create('Bert', 20);
  Attempt(bank, 'Anna', 'Bert', 30);
  Attempt(bank, 'Bert', 'Anna', 500);
  Attempt(bank, 'Anna', 'Carla', 10);
  Attempt(bank, 'Anna', 'Bert', -5);

  if bank.Find('Bert') is ok nr then WriteLn('Bert has account no. ', nr);
  WriteLn('Carla: ', bank.Find('Carla').ValueOr(0));
  if bank.Withdraw(1, 1000).IsError then WriteLn('cannot withdraw 1000');
end.
