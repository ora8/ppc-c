{ EXPECT: the field "FSecret" of "TBase" is private }
program T;
type
  TBase = class
  private
    FSecret: Integer;
    procedure Hidden;
  protected
    FShared: Integer;
    procedure Helper;
  public
    constructor Create;
    function Secret: Integer; const;
    property Shared: Integer read FShared;
  end;
  TChild = class(TBase)
  public
    procedure Work;
  end;
  TPoint = record
  private
    FX: Integer;
  public
    procedure SetX(v: Integer);
    function X: Integer; const;
  end;
constructor TBase.Create; begin FSecret := 42; FShared := 1; Hidden; end;
procedure TBase.Hidden; begin Inc(FSecret); end;
procedure TBase.Helper; begin Inc(FShared); end;
function TBase.Secret: Integer; const; begin Result := FSecret; end;
procedure TChild.Work;
  procedure Inner; begin Helper; end;
begin
  Inc(FShared);
  Inner;
  Helper;
end;
procedure TPoint.SetX(v: Integer); begin FX := v; end;
function TPoint.X: Integer; const; begin Result := FX; end;
val p: TPoint := default;
begin vao c := TBase.Create; begin WriteLn(c.FSecret); end; end.
