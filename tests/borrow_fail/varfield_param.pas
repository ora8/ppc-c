{ EXPECT: cannot store the reference to 'p' in the 'var' field 'c.Parent' }
program T;
type
  TNode = class
    Name: String;
    Parent: var optional TNode;
    Child: optional TNode;
    constructor Create(AName: String);
  end;
constructor TNode.Create(AName: String); begin Name := AName; end;
procedure Link(var c, p: TNode); begin c.Parent := p; end; begin end.
