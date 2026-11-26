{* EXPECT: Runtime error 227: assertion failed at runtime_fail/assert_failed.pas(5,3): the list must not be empty *}
program T;
val n: Integer := 0;
begin
  Assert(n > 0, 'the list must not be empty');
end.
