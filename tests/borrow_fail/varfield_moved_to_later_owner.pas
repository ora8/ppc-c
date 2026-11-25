{ EXPECT: cannot be moved into 'd', which is freed after 'a' (declare 'a' before 'd') }
program T;
type
  TNode = class
    Name: String;
    Parent: var optional TNode;
    Child: optional TNode;
    constructor Create(AName: String);
  end;
constructor TNode.Create(AName: String); begin Name := AName; end;
begin vao d := TNode.Create('d'); vao a := TNode.Create('a'); begin begin vao c := TNode.Create('c'); begin c.Parent := a; d := c; end; end; end; end.
