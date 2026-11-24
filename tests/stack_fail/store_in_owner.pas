{ EXPECT: cannot store borrowed reference 's' in owning variable 'o' }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; val s: TFoo; var o: TFoo; begin s.Create(1); o := s; end;
begin P; end.
