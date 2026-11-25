{ EXPECT: 'c' refers to 'a' through a 'var' field and cannot be moved into parameter 'x' of 'P' }
program T;
type
  TNode = class
    Name: String;
    Parent: var optional TNode;
    Child: optional TNode;
    constructor Create(AName: String);
  end;
constructor TNode.Create(AName: String); begin Name := AName; end;
procedure P(vao x: TNode); begin end; begin vao a := TNode.Create('a'); vao c := TNode.Create('c'); begin c.Parent := a; P(c); end; end.
