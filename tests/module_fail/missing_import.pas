{* EXPECT: module 'Nowhere' not found (looked for module_fail/Nowhere.pas and module_fail/Nowhere.pps); other directories are given with --module-paths dir1:dir2 *}
module missing_import;
import Nowhere;
interface
implementation
end.
