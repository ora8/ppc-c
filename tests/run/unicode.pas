program UnicodeStrings;
{* strings are UTF-8 bytes; UChar is one character (a code point); the U... routines work on characters *}
import std;
val s := 'Grüße, κόσμε!';
val c: UChar := 'ß';
val n := 0;
val euro := UChars('abc€');
begin
  WriteLn(Length(s), ' bytes, ', ULength(s), ' characters');
  for val ch in UChars(s) do Write(ch, '|');
  WriteLn;
  WriteLn(UCopy(s, 3, 4), ' ', UUpperCase(s), ' ', ULowerCase('ÄÖÜ ΑΒΓ'));
  WriteLn(UAt(s, 3) = 'ü', ' ', UAt(s, 9), ' ', UPos('κ', s), ' ', Pos('κ', s));
  WriteLn(Ord(c), ' ', UChr(946), ' ', s + c, ' ', UReverse('añb'));
  WriteLn(UIsLetter('ö'), ' ', UIsLetter('3'), ' ', UIsUpper('Ä'), ' ', UUpCase('é'));
  WriteLn(IsValidUTF8(s), ' ', IsValidUTF8(Copy(s, 1, 3)));
  for val ch in UChars(s) do
    case ch of
      'ü', 'ß': Inc(n);
    else
    end;
  WriteLn(n, ' german letters; ', Format('%s=%d', [c, Ord(c)]));
  WriteLn(UString(euro), ' ', Ord(High(UChar)));
end.
