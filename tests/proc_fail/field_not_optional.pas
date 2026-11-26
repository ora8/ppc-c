{* EXPECT: record field of type "procedure()" must be declared 'optional procedure()' (this storage starts out empty) *}
program T;
type TF = procedure; TR = record F: TF; end;
begin end.
