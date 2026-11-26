{* EXPECT: 'owned' was replaced by 'vao' (for example 'procedure Add(vao Item: TItem)') *}
program T;
type TK = class end; procedure P(owned x: TK); begin end;
begin end.
