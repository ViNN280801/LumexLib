#include <cstdio>
#include <exception>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "lumex/applied/json/LumexJson"
#include "lumex/core/string_view/view/LumexStringView.hpp"

using lumex::applied::json::diagnostics::LumexJsonDiagnosticLevel;
using lumex::applied::json::diagnostics::set_diagnostic_reporter;
using lumex::applied::json::helper::LumexJsonHelper;
using lumex::applied::json::normalization::LumexJsonSchemaNormalizer;
using lumex::applied::json::schema::LumexJsonSchemaException;
using lumex::applied::json::schema::LumexJsonSchemaTraverser;
using lumex::applied::json::validation::LumexJsonSchemaValidator;
using lumex::core::string_view::view::LumexStringView;

namespace
{
void
print_diagnostic (LumexJsonDiagnosticLevel level, char const *message)
{
  std::cout << "  (diagnostic " << toString (level) << ") " << message << '\n';
}
} // namespace

int
main ()
{
  set_diagnostic_reporter (&print_diagnostic);
  std::string const path ("lumex_json_example.json");
  std::remove (path.c_str ());

  std::cout << "=== 1. LumexJsonHelper: write, read, edit ===\n";
  bool const written
      = LumexJsonHelper::write_value (path, "host", "127.0.0.1", "network");
  bool const port_written
      = LumexJsonHelper::write_value (path, "port", 5672, "network");
  nlohmann::json config = LumexJsonHelper::load_config (path);
  std::cout << "written=" << written << port_written << " host="
            << LumexJsonHelper::get_value (config, "host", "?", "network")
            << " port="
            << LumexJsonHelper::get_value (config, "port", 0, "network")
            << '\n';
  bool const edited
      = LumexJsonHelper::edit_value (path, "port", 5673, "network");
  bool const missing_edit
      = LumexJsonHelper::edit_value (path, "absent", 1, "network");
  std::cout << "edit existing=" << edited << " edit missing=" << missing_edit
            << '\n';

  std::cout << "\n=== 2. Nested section paths ===\n";
  LumexJsonHelper::set_value (config, "level", "debug", "logging.console");
  std::cout << "logging.console.level="
            << LumexJsonHelper::get_value (config, "level", "?",
                                           "logging.console")
            << " has_key="
            << LumexJsonHelper::has_key (config, "level", "logging.console")
            << '\n';

  std::cout << "\n=== 3. Schema validation ===\n";
  nlohmann::json const schema = nlohmann::json::parse (R"({
    "type": "object", "required": ["id", "name"],
    "properties": { "id": { "type": "integer" },
                    "name": { "type": "string" } } })");
  LumexJsonSchemaValidator const strict (schema);
  std::string const good ("{\"id\": 1, \"name\": \"pump\", \"x\": true}");
  std::string const bad ("{\"id\": \"one\", \"name\": \"pump\"}");
  strict.validate (LumexStringView (good));
  std::cout << "good document passed\n";
  try
    {
      strict.validate (LumexStringView (bad));
    }
  catch (LumexJsonSchemaException const &exc)
    {
      std::cout << "strict: " << exc.what () << '\n';
    }
  LumexJsonSchemaValidator const lenient (schema, false);
  std::exception_ptr error;
  lenient.validate (LumexStringView (bad), error);
  std::cout << "lenient: "
            << LumexJsonSchemaNormalizer::get_exception_message (error)
            << '\n';

  std::cout << "\n=== 4. Normalization through the walker ===\n";
  nlohmann::json const normalized = LumexJsonSchemaTraverser::run (
      schema, nlohmann::json::parse ("{\"id\": 2, \"extra\": 1}"));
  std::cout << "normalized=" << normalized.dump () << '\n';

  std::remove (path.c_str ());
  set_diagnostic_reporter (nullptr);
  bool const ok = written && port_written && edited && !missing_edit
                  && normalized["name"].is_null () && error;
  return ok ? 0 : 1;
}
