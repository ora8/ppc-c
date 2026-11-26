{* EXPECT: a field cannot be "reference to procedure()" *}
program T;
type TBtn = class OnClick: reference to procedure; end;
begin end.
