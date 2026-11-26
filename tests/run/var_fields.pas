program var_fields;
type
  TNode = class
    Name: String;
    Parent: var optional TNode;          {* reference without ownership *}
    Kids: array of optional TNode;       {* owned *}
    constructor Create(AName: String);
    destructor Destroy; override;
    procedure Add(vao c: TNode);
    function Path: String; const;
  end;
constructor TNode.Create(AName: String); begin Name := AName; end;
destructor TNode.Destroy; begin WriteLn('free ', Name); end;
procedure TNode.Add(vao c: TNode);
begin
  c.Parent := Self;                      {* c ends up inside Self *}
  SetLength(Kids, Length(Kids) + 1);
  Kids[High(Kids)] := c;
end;
function TNode.Path: String; const;
begin
  Result := Name;
  if Parent is some p then Result := p.Path + '/' + Name;
end;

begin
  vao root := TNode.Create('root');
  vao a := TNode.Create('a');
  vao b := TNode.Create('b');
  begin
    root.Add(TNode.Create('x'));
    a.Parent := root;                    {* a is declared after root: freed first *}
    b.Parent := a;
    WriteLn(b.Path);
    b.Parent := root;                    {* b no longer refers to a *}
    root.Add(a);                         {* a moves into root, which it refers to *}
    WriteLn(b.Path);
    for val k in root.Kids do WriteLn(k.Path);
    vao c := TNode.Create('c');
    begin
      c.Parent := root;
      WriteLn(c.Path);
    end;
  end;
  WriteLn('end');
end.
