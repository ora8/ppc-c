{* EXPECT: the record field 'Name' is a String *}
library T; type TR = record Name: String; end; procedure F(r: TR); begin end; exports F; begin end.
