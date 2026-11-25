{ EXPECT: cannot store the reference to 'p' in the 'var' field }
program T;
type
  TNode = class
    Name: String;
    Parent: var optional TNode;
    Child: optional TNode;
    constructor Create(AName: String);
  end;
constructor TNode.Create(AName: String); begin Name := AName; end;
type TLeaf = class Up: var optional TNode; procedure Attach(var p: TNode); end; procedure TLeaf.Attach(var p: TNode); begin Up := p; end; begin end.
