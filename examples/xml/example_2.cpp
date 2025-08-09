#include <iostream>

#include "lumex/xml/LumexXml"

static constexpr char const *kXmlFilePath = "xgconsole.xml";

using namespace Lumex::Xml;
using namespace Lumex::Xml::Types;

int
main()
{
  // --- Part 1: Create and Save an XML file using LumexXml ---
  std::cout << "--- Creating and Saving XML File (LumexXml) ---\n";
  Document::XmlDocument docWrite;

  // Create XML manually to test serialization
  auto profile = docWrite.append_child("Profile");
  auto tools   = profile.append_child("Tools");

  auto tool1   = tools.append_child("Tool");
  tool1.append_attribute("Filename").set_value("tool1.exe");
  tool1.append_attribute("Timeout").set_value("100");

  auto tool2 = tools.append_child("Tool");
  tool2.append_attribute("Filename").set_value("tool2.exe");
  tool2.append_attribute("Timeout").set_value("0");

  // Save to file with explicit text mode
  if(!docWrite.save_file(kXmlFilePath, "\t", Constants::kformat_save_file_text | Constants::kformat_no_escapes,
                         encoding_utf8))
  {
    std::cerr << "Error saving XML file: " << kXmlFilePath << "\n";
    return -1;
  }
  std::cout << "XML file '" << kXmlFilePath << "' created and saved successfully.\n\n";

  // --- Part 2: Example 1 - Reading and Iterating through nodes (LumexXml) ---
  std::cout << "--- Example 1: Reading and Iterating through nodes (LumexXml) ---\n";
  Document::XmlDocument docRead;
  XmlParseResult resultRead = docRead.load_file(kXmlFilePath);

  if(!resultRead)
  {
    std::cerr << "Error loading XML file: " << resultRead.description() << "\n";
    return -1;
  }

  // Traverse nodes directly
  for(Node::XmlNode tool : docRead.child("Profile").child("Tools").children("Tool"))
  {
    // Debug: print all attributes
    std::cout << "DEBUG: Tool node attributes:\n";
    for(auto attr : tool.attributes())
    {
      std::cout << "  - Name: '" << ((attr.name() != nullptr) ? attr.name() : "NULL") << "'\n";
      std::cout << "  - Value: '" << ((attr.value() != nullptr) ? attr.value() : "NULL") << "'\n";
    }

    int timeout = tool.attribute("Timeout").as_int();

    if(timeout > 0) std::cout << "Tool " << tool.attribute("Filename").value() << " has timeout " << timeout << "\n";
  }
  std::cout << "\n";

  // --- Part 3: Example 2 - Reading and Querying with XPath (LumexXml) ---
  std::cout << "--- Example 2: Reading and Querying with XPath (LumexXml) ---\n";

  // Select nodes using XPath
  XPathNodeSet tools_with_timeout = docRead.select_nodes("/Profile/Tools/Tool[@Timeout > 0]");

  for(XPathNode node : tools_with_timeout)
  {
    XmlNode tool = node.node();
    std::cout << "Tool " << tool.attribute("Filename").value() << " has timeout " << tool.attribute("Timeout").as_int()
              << "\n";
  }

  return 0;
}
