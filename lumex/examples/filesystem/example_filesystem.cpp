#include <fstream>
#include <iostream>
#include <string>

#include "lumex/core/filesystem/LumexFilesystem"

using namespace lumex::core::filesystem::fs;

int
main ()
{
  std::cout << "=== Filesystem paths, files, directories ===\n\n";

  std::cout << "--- 1. Current and temp directories ---\n";
  filesystem_result<path> const cwd = lumex_filesystem::current_path ();
  filesystem_result<path> const tmp = lumex_filesystem::temp_directory_path ();
  if (!cwd || !tmp)
    {
      std::cerr << "cannot resolve current_path or temp_directory_path\n";
      return 1;
    }
  std::cout << "cwd=" << cwd.value ().string () << '\n';
  std::cout << "tmp=" << tmp.value ().string () << '\n';

  path const sandbox = tmp.value () / "lumex_fs_example";
  filesystem_result<bool> const created
      = lumex_filesystem::create_directories (sandbox / "nested");
  std::cout << "create_directories ok=" << (created ? "yes" : "no") << '\n';

  std::cout << "\n--- 2. Write, size, copy, rename ---\n";
  path const src = sandbox / "run.txt";
  {
    std::ofstream out (src.string ().c_str ());
    out << "chromatogram\n";
  }
  filesystem_result<std::uintmax_t> const size
      = lumex_filesystem::file_size (src);
  std::cout << "src exists=" << (lumex_filesystem::exists (src) ? "yes" : "no")
            << " size=" << (size ? size.value () : 0) << '\n';

  path const copy = sandbox / "run.copy.txt";
  filesystem_result<void> const copied
      = lumex_filesystem::copy_file (src, copy);
  path const renamed = sandbox / "run.renamed.txt";
  filesystem_result<void> const moved
      = lumex_filesystem::rename (copy, renamed);
  std::cout << "copy ok=" << (copied ? "yes" : "no")
            << " rename ok=" << (moved ? "yes" : "no") << '\n';

  std::cout << "\n--- 3. path observers ---\n";
  path const abs = lumex_filesystem::absolute (src);
  std::cout << "absolute=" << abs.string ()
            << " is_absolute=" << (abs.is_absolute () ? "yes" : "no")
            << " filename=" << abs.filename ().string ()
            << " parent=" << abs.parent_path ().string () << '\n';

  std::cout << "\n--- 4. directory_entry + directory_iterator ---\n";
  directory_entry const entry (src);
  std::cout << "entry path=" << entry.path ().string ()
            << " exists=" << (entry.exists () ? "yes" : "no")
            << " regular=" << (entry.is_regular_file () ? "yes" : "no")
            << " dir=" << (entry.is_directory () ? "yes" : "no") << '\n';

  directory_iterator it (sandbox);
  directory_iterator const end;
  for (; it != end; ++it)
    {
      std::cout << "  child=" << it->path ().filename ().string ()
                << " dir=" << (it->is_directory () ? "yes" : "no") << '\n';
    }

  std::cout << "\n--- 5. Cleanup ---\n";
  filesystem_result<std::uintmax_t> const removed
      = lumex_filesystem::remove_all (sandbox);
  std::cout << "remove_all count=" << (removed ? removed.value () : 0)
            << " leftover="
            << (lumex_filesystem::exists (sandbox) ? "yes" : "no") << '\n';

  std::cout << "\n=== Filesystem example finished ===\n";
  return 0;
}
