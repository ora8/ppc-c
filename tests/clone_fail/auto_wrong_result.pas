{* EXPECT: Clone of "TA" must return "TA" or one of its ancestor classes *}
program T;
type TB = class end; TA = class function Clone: TB; const; auto; end;
begin end.
