{* EXPECT: 'b' still refers to another object through a 'var' field when it is freed at the end of its block *}
program T;
type
  TNode = class
    Name: String;
    Parent: var optional TNode;
    Child: optional TNode;
    constructor Create(AName: String);
  end;
constructor TNode.Create(AName: String); begin Name := AName; end;
begin vao a := TNode.Create('a'); vao b := TNode.Create('b'); begin b.Parent := a; a := TNode.Create('x'); end; end.
