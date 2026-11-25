{ EXPECT: the field "FSecret" of "TBase" is private }
program T;
type
  TBase = class
  private
    FSecret: Integer;
  end;
  TChild = class(TBase)
    procedure Work;
  end;
procedure TChild.Work; begin Inc(FSecret); end;
begin end.
