program indexers;
type
  TGrid = class
    Cells: array of Integer;
    W: Integer;
    constructor Create(AW, AH: Integer);
    function GetCell(x, y: Integer): Integer; const;
    procedure SetCell(x, y: Integer; v: Integer);
    property Cell[x, y: Integer]: Integer read GetCell write SetCell; default;
    function Sum: Integer; const;
  end;
  TNames = record
    List: array of String;
    function Get(i: Integer): String; const;
    property Name[i: Integer]: String read Get;
  end;
constructor TGrid.Create(AW, AH: Integer); begin W := AW; SetLength(Cells, AW * AH); end;
function TGrid.GetCell(x, y: Integer): Integer; const; begin Result := Cells[y * W + x]; end;
procedure TGrid.SetCell(x, y: Integer; v: Integer); begin Cells[y * W + x] := v; end;
function TGrid.Sum: Integer; const;
begin
  Result := 0;
  for val y := 0 to 1 do
    for val x := 0 to W - 1 do Result := Result + Cell[x, y];
end;
function TNames.Get(i: Integer): String; const; begin Result := List[i]; end;
val n: TNames := (List: ('anna', 'bert'));
begin
  vao g := TGrid.Create(3, 2);
  begin
    g[0, 0] := 5;
    g.Cell[2, 1] := 7;
    g[1, 1] := g[0, 0] + 1;
    WriteLn(g[0, 0], ' ', g.Cell[2, 1], ' ', g[1, 1], ' ', g.Sum);
  end;
  WriteLn(n.Name[1]);
end.
