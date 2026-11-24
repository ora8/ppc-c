{ EXPECT: objects are created in the initializer: 'vao x := TK.Create(...)' }
program T;
type TK = class V: Integer; constructor Create; class function Neu: TK; end; constructor TK.Create; begin inherited Create; end; class function TK.Neu: TK; begin Result := TK.Create; end;
vao k: TK.Create;
begin end.
