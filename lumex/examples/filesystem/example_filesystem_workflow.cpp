#include <fstream>
#include <iostream>
#include <vector>

#include "lumex/core/filesystem/LumexFilesystem"

using namespace lumex::core::filesystem::fs;

int
main ()
{
  std::cout
      << "=== Workflow: stage a method file next to a data folder ===\n\n";

  filesystem_result<path> const tmp = lumex_filesystem::temp_directory_path ();
  if (!tmp)
    {
      std::cerr << "temp_directory_path failed\n";
      return 1;
    }

  path const root = tmp.value () / "lumex_fs_workflow";
  path const data = root / "data";
  path const methods = root / "methods";
  lumex_filesystem::create_directories (data);
  lumex_filesystem::create_directories (methods);

  path const method = methods / "isocratic.ini";
  {
    std::ofstream out (method.string ().c_str ());
    out << "[pump]\nflow=1.0\n";
  }

  path const staged = data / method.filename ();
  lumex_filesystem::copy_file (method, staged);

  filesystem_result<std::vector<path>> const listing
      = lumex_filesystem::directory_paths (data);
  std::cout << "data_entries=" << (listing ? listing.value ().size () : 0)
            << " staged_exists="
            << (lumex_filesystem::exists (staged) ? "yes" : "no") << '\n';

  lumex_filesystem::remove_all (root);
  return 0;
}
