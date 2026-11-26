{* EXPECT: cannot store borrowed reference 'r' in owning variable 'h'; declare 'h' with 'var' (reference) or transfer ownership *}
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; val s := TFoo.Create(1); var r: TFoo := s; vao h: TFoo := r; begin end;
begin P; end.
