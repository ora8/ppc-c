program BorrowErrors;
{ Intentionally wrong: every marked line is rejected by the borrow
  checker. Compile with:  ./ppc examples/borrow_errors.pas }
type
  TAccount = class
    Balance: Integer;
  end;

var
  Global: optional TAccount := none;

procedure CloseIt(K: TAccount);          { K is only borrowed }
begin
  K.Free;                                { ERROR 1: freeing a borrow }
end;

procedure Remember;
vao account := TAccount.Create;              { freed at the end of Remember }
begin
  Global := account;                       { ERROR 2: a reference to a local owner escapes }
end;

procedure Swap(var A, B: Integer);
val t: Integer := default;
begin t := A; A := B; B := t; end;

vao
  a := TAccount.Create;
  b: TAccount := a;                        { Move: a -> b }
var
  view: TAccount := b;                    { borrow of b (does not own) }
val
  x: Integer := default;
begin
  a.Balance := 100;                        { ERROR 3: a was moved }
  b.Free;
  WriteLn(view.Balance);                  { ERROR 4: owner already freed }

  b.Free;                                { ERROR 5: double Free }

  x := 1;
  Swap(x, x);                         { ERROR 6: two var borrows of x }

  a := TAccount.Create;
  a := TAccount.Create;                    { no error: the first object is freed automatically }
  a.Free;
end.
