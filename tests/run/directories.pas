program directories;
import std;
val base := 'ppc_dir_test.tmp';
begin
  if ForceDirs(JoinPath(base, 'a/b/c')) is error e then WriteLn(e);
  if CreateDir(JoinPath(base, 'x')) is ok then WriteLn('created x');
  if CreateDir(JoinPath(base, 'x')) is error e then WriteLn('again: ', Copy(e, 1, 27));
  if WriteFile(JoinPath(base, 'notes.txt'), 'hello') is ok then WriteLn('written');
  val r := ListDir(base);
  begin
    if r is ok names then for val n in names do WriteLn('  ', n, ' ', DirExists(JoinPath(base, n)));
  end;
  if FileSize(JoinPath(base, 'notes.txt')) is ok sz then WriteLn('size ', sz);
  if RenameFile(JoinPath(base, 'notes.txt'), JoinPath(base, 'n2.txt')) is ok then WriteLn('renamed ', FileExists(JoinPath(base, 'n2.txt')));
  if ListDir('/no/such/dir') is error e then WriteLn(e);
  WriteLn(ExtractFileName('src/main.pas'), ' ', ExtractFileDir('src/main.pas'), ' ', ExtractFileExt('src/main.pas'), ' ',
          ChangeFileExt('src/main.pas', '.c'), ' [', ExtractFileExt('.bashrc'), '] ', ExtractFileDir('/x'), ' ', JoinPath('a/', 'b'));
  val here := CurrentDir;
  begin
    if SetCurrentDir(base) is ok then WriteLn('in ', ExtractFileName(CurrentDir));
    if SetCurrentDir(here) is ok then WriteLn('back');
  end;
  if DeleteFile(JoinPath(base, 'n2.txt')) is ok then;
  val dirs: array of String := ('a/b/c', 'a/b', 'a', 'x', '');
  begin
    for val d in dirs do
      if RemoveDir(JoinPath(base, d)) is error e then WriteLn(e);
  end;
  WriteLn(DirExists(base));
end.
