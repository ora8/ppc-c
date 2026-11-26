{* EXPECT: class methods cannot be virtual, abstract or override *}
program T;
type TK = class class function F: Integer; virtual; end;
begin end.
