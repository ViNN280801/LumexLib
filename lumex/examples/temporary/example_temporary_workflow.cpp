#include <fstream>
#include <iostream>

#include "lumex/core/temporary/LumexTemporary"

using namespace lumex::core::temporary::tmp;

int
main ()
{
  std::cout << "=== Workflow: stage an export in a temp directory ===\n\n";

  lumex::filesystem_result<TemporaryDirectory> dir
      = LumexTemporary::create_temp_directory ("export");
  if (!dir || !dir.value ().is_valid ())
    {
      std::cerr << "cannot create export directory\n";
      return 1;
    }

  lumex::path const out = dir.value ().path () / "peaks.csv";
  {
    std::ofstream csv (out.string ().c_str ());
    csv << "rt,area\n1.23,45000\n";
  }
  std::cout << "export=" << out.string () << '\n';
  return 0;
}
