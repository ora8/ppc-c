{* EXPECT: none can only be stored in an optional type, not in "TFoo" *}
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
procedure Use(x: TFoo); begin WriteLn(x.V); end;
begin Use(none); end.
