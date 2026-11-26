{* EXPECT: the file declares 'module OtherName', but it is imported as 'WrongName' *}
program T;
import WrongName;
begin end.
