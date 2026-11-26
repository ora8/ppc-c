program ConstMethods;
{* methods with '; const;': do not change Self;
  and routines without side effects on global variables *}
type
  TVector = record
    X, Y: Real;
    procedure Scale(F: Real);
    function Len: Real; const;               {* does not change Self *}
    function Normalized: TVector; const;
    procedure Show; const;
  end;

  TAccount = class
  private
    FBalance: Integer;
  public
    constructor Create(S: Integer);
    procedure Deposit(B: Integer);
    function Balance: Integer; const;
    function Description: String; const; virtual;
  end;
  TSavings = class(TAccount)
    function Description: String; const; override; {* the override stays const *}
  end;

procedure TVector.Scale(F: Real); begin X := X * F; Y := Y * F; end;
function TVector.Len: Real; const; begin Result := Sqrt(X * X + Y * Y); end;
function TVector.Normalized: TVector; const;
val l := Len;                                 {* const method calls a const method *}
begin
  Result.X := X / l;
  Result.Y := Y / l;
end;
procedure TVector.Show; const; begin WriteLn('(', X:0:2, ', ', Y:0:2, ') Len ', Len:0:2); end;

constructor TAccount.Create(S: Integer); begin inherited Create; FBalance := S; end;
procedure TAccount.Deposit(B: Integer); begin FBalance := FBalance + B; end;
function TAccount.Balance: Integer; const; begin Result := FBalance; end;
function TAccount.Description: String; const; begin Result := ClassName + ' ' + IntToStr(Balance); end;
function TSavings.Description: String; const; begin Result := 'Spar: ' + inherited Description; end;

{* free const function: only parameters and local variables, no side effects *}
function SquareOf(N: Integer): Integer; const; begin Result := N * N; end;
function SumOfSquares(N: Integer): Integer; const;
begin
  Result := 0;
  for val i := 1 to N do Result := Result + SquareOf(i);   {* calls only const routines *}
end;

procedure Print(V: TVector const);         {* T const: only const methods *}
begin
  V.Show;
  V.Normalized.Show;
end;

val
  v: TVector := default;
vao
  k := TSavings.Create(100);
begin
  v.X := 3; v.Y := 4;
  Print(v);
  v.Scale(2);                                {* v itself is not constant *}
  v.Show;
  k.Deposit(50);
  WriteLn(k.Description, ', Balance ', k.Balance);
  WriteLn('SumOfSquares(4) = ', SumOfSquares(4));
end.
