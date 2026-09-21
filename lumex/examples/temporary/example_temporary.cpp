#include <fstream>
#include <iostream>

#include "lumex/core/temporary/LumexTemporary"

using namespace lumex::core::temporary::tmp;

int
main ()
{
  std::cout << "=== Temporary directories and files ===\n\n";

  std::cout << "--- 1. Temp root and generated name ---\n";
  lumex::path const root = LumexTemporary::get_temp_directory_path ();
  std::string const name = LumexTemporary::generate_temp_name ("ex");
  std::cout << "temp_dir=" << root.string () << " name=" << name << '\n';

  std::cout << "\n--- 2. RAII directory ---\n";
  lumex::filesystem_result<TemporaryDirectory> dir
      = LumexTemporary::create_temp_directory ("lumex_ex");
  if (!dir)
    {
      std::cerr << "create_temp_directory failed code=" << dir.error_code ()
                << '\n';
      return 1;
    }
  std::cout << "dir=" << dir.value ().path ().string ()
            << " valid=" << (dir.value ().is_valid () ? "yes" : "no") << '\n';

  std::cout << "\n--- 3. RAII file ---\n";
  lumex::filesystem_result<TemporaryFile> file
      = LumexTemporary::create_temp_file ("lumex_ex");
  if (!file)
    {
      std::cerr << "create_temp_file failed code=" << file.error_code ()
                << '\n';
      return 1;
    }
  {
    std::ofstream out (file.value ().path ().string ().c_str ());
    out << "scratch\n";
  }
  std::cout << "file=" << file.value ().path ().string ()
            << " valid=" << (file.value ().is_valid () ? "yes" : "no") << '\n';

  std::cout << "\n--- 4. Destructors remove the scratch paths ---\n";
  std::cout << "leaving scope; TemporaryDirectory/File unlink themselves\n";

  std::cout << "\n=== Temporary example finished ===\n";
  return 0;
}
