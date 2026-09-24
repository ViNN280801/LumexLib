// Workflow: a per-operation normalizer that upgrades a legacy request field
// and then shapes the request with a schema, used in non-strict mode so a bad
// request is reported instead of thrown.
#include <exception>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "lumex/applied/json/LumexJson"
#include "lumex/core/string_view/view/LumexStringView.hpp"

using lumex::applied::json::normalization::LumexJsonSchemaNormalizer;
using lumex::core::string_view::view::LumexStringView;

namespace
{
class StartRunNormalizer final : public LumexJsonSchemaNormalizer
{
public:
  explicit StartRunNormalizer (bool strict)
      : LumexJsonSchemaNormalizer (strict),
        _schema (nlohmann::json::parse (R"({
          "type": "object", "required": ["method", "volume"],
          "properties": {
            "method": { "type": "string" },
            "volume": { "type": "number" },
            "mode":   { "type": "string", "enum": ["fast", "precise"] } } })"))
  {
  }

  nlohmann::json
  normalize (LumexStringView raw) const override
  {
    std::exception_ptr ignored;
    return normalize (raw, ignored);
  }

  nlohmann::json
  normalize (LumexStringView raw, std::exception_ptr &error) const override
  {
    error = nullptr;
    try
      {
        return normalize_against_schema (_schema, _upgrade (parse (raw)));
      }
    catch (std::exception const &)
      {
        report_or_rethrow (error);
      }
    return nlohmann::json ();
  }

  void
  validate (LumexStringView raw) const override
  {
    std::exception_ptr ignored;
    validate (raw, ignored);
  }

  void
  validate (LumexStringView raw, std::exception_ptr &error) const override
  {
    error = nullptr;
    try
      {
        validate_against_schema (_schema, _upgrade (parse (raw)));
      }
    catch (std::exception const &)
      {
        report_or_rethrow (error);
      }
  }

private:
  // Old clients send "injectionVolume" instead of "volume".
  static nlohmann::json
  _upgrade (nlohmann::json request)
  {
    if (request.is_object () && request.contains ("injectionVolume"))
      {
        request["volume"] = request["injectionVolume"];
        request.erase ("injectionVolume");
      }
    return request;
  }

  nlohmann::json _schema;
};
} // namespace

int
main ()
{
  StartRunNormalizer const normalizer (false);
  char const *const requests[] = {
    "{\"method\": \"A\", \"volume\": 10, \"mode\": \"fast\"}",
    "{\"method\": \"B\", \"injectionVolume\": 5.5, \"debug\": true}",
    "{\"method\": \"C\", \"volume\": 1, \"mode\": \"turbo\"}",
    "{\"method\": ",
  };

  int accepted = 0;
  int rejected = 0;
  for (char const *const request : requests)
    {
      std::string const raw (request);
      std::exception_ptr error;
      nlohmann::json const result
          = normalizer.normalize (LumexStringView (raw), error);
      if (error)
        {
          ++rejected;
          std::cout << "rejected: "
                    << LumexJsonSchemaNormalizer::get_exception_message (error)
                    << '\n';
        }
      else
        {
          ++accepted;
          std::cout << "accepted: " << result.dump () << '\n';
        }
    }
  std::cout << accepted << " accepted, " << rejected << " rejected\n";
  return accepted == 2 && rejected == 2 ? 0 : 1;
}
