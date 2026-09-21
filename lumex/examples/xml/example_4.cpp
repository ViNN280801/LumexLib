#include <iostream>

#include "lumex/core/utility/macros/LumexConstantMacros.hpp"
#include "lumex/xml/LumexXml"

using namespace lumex::xml;
using namespace lumex::xml::constants;
using namespace lumex::xml::document;
using namespace lumex::xml::node;
using namespace lumex::xml::text;
using namespace lumex::xml::types::Types;
using namespace lumex::xml::xpath::node;

LUMEX_CONST_STR kXmlFilePath = "lumex_xml_example_4.xml";

int
main ()
{
  std::cout << "=== Workflow: chromatogram peak table as XML ===\n\n";

  XmlDocument doc;
  XmlNode root = doc.append_child ("Chromatogram");
  root.append_attribute ("instrument").set_value ("HPLC-01");

  XmlNode peaks = root.append_child ("Peaks");
  XmlNode p1 = peaks.append_child ("Peak");
  p1.append_attribute ("rt").set_value ("1.23");
  p1.append_attribute ("area").set_value ("45000");
  XmlNode p2 = peaks.append_child ("Peak");
  p2.append_attribute ("rt").set_value ("4.80");
  p2.append_attribute ("area").set_value ("1200");

  if (!doc.save_file (kXmlFilePath, "\t",
                      Constants::kformat_save_file_text
                          | Constants::kformat_no_escapes,
                      encoding_utf8))
    {
      std::cerr << "save_file failed\n";
      return 1;
    }

  XmlDocument loaded;
  xml_parse_result_t const parsed = loaded.load_file (kXmlFilePath);
  if (!parsed)
    {
      std::cerr << "load_file: " << parsed.description () << '\n';
      return 1;
    }

  XPathNodeSet const large
      = loaded.select_nodes ("/Chromatogram/Peaks/Peak[@area > 10000]");
  std::cout << "large_peaks=" << large.size () << '\n';
  for (XPathNode node : large)
    {
      XmlNode peak = node.node ();
      std::cout << "  rt=" << peak.attribute ("rt").value ()
                << " area=" << peak.attribute ("area").value () << '\n';
    }
  return 0;
}
