program Comments;
{* comments over several lines:
   a '{' or '}' inside a comment is fine: { } *}
(* the paren-star form
   also works *)
val s: String := '{ not a comment } {* nor this *}';   // strings keep their braces
begin
  WriteLn(s);  {* a comment after a statement *}
  WriteLn(1 {* inside an expression *} + 2);
end.
