#include <iostream>
#include <string>

#include "lumex/xml/LumexXml"

static constexpr char const *kXmlFilePath = "file_for_example_3.xml";

using namespace Lumex::Xml;
using namespace Lumex::Xml::Types;

/**
 * @brief Main function demonstrating various LumexXml functionalities.
 *
 * This example showcases loading an XML file, navigating its structure,
 * accessing and manipulating attributes, and performing XPath queries.
 *
 * @return 0 on success, -1 on error.
 */
int
main()
{
  std::cout << "--- Part 1: Loading XML File ---\n";
  Document::XmlDocument docRead;
  XmlParseResult resultRead = docRead.load_file(kXmlFilePath);

  if(!resultRead)
  {
    std::cerr << "Error loading XML file '" << kXmlFilePath << "': " << resultRead.description() << "\n";
    return -1;
  }
  std::cout << "XML file '" << kXmlFilePath << "' loaded successfully.\n\n";

  std::cout << "--- Part 2: Basic Node Traversal and Attribute Access ---\n";
  Node::XmlNode projectRoot = docRead.document_element();
  if(!projectRoot)
  {
    std::cerr << "Error: Root 'Project' element not found.\n";
    return -1;
  }
  std::cout << "Root element name: " << projectRoot.name() << "\n";
  std::cout << "Root element default namespace: " << projectRoot.attribute("xmlns").value() << "\n\n";

  // Iterate through ProjectConfiguration groups
  Node::XmlNode projectConfigurations = projectRoot.child("ItemGroup");
  if(projectConfigurations && !projectConfigurations.children("ProjectConfiguration").empty())
  {
    std::cout << "Project Configurations:\n";
    for(Node::XmlNode config : projectConfigurations.children("ProjectConfiguration"))
    {
      std::string configuration = config.child("Configuration").text().get();
      std::string platform      = config.child("Platform").text().get();
      std::cout << "  - Configuration: " << configuration << ", Platform: " << platform << "\n";
    }
  }
  std::cout << "\n";

  // Navigate to a specific PropertyGroup and extract global properties
  Node::XmlNode globalsPropertyGroup = docRead.child("Project").child("PropertyGroup");
  if(globalsPropertyGroup != nullptr)
  {
    std::cout << "Global Properties:\n";
    std::cout << "  VCProjectVersion: " << globalsPropertyGroup.child("VCProjectVersion").text().get() << "\n";
    std::cout << "  Keyword: " << globalsPropertyGroup.child("Keyword").text().get() << "\n";
    std::cout << "  RootNamespace: " << globalsPropertyGroup.child("RootNamespace").text().get() << "\n";
    std::cout << "  WindowsTargetPlatformVersion: "
              << globalsPropertyGroup.child("WindowsTargetPlatformVersion").text().get() << "\n";
  }
  std::cout << "\n";

  std::cout << "--- Part 3: Advanced Node and Attribute Selection with XPath ---\n";

  // XPath 1: Select all ProjectConfiguration nodes with Platform "x64"
  XPathNodeSet x64Configs = docRead.select_nodes("/Project/ItemGroup/ProjectConfiguration[Platform='x64']");
  std::cout << "x64 Configurations (" << x64Configs.size() << " found):\n";
  for(XPathNode xPathNode : x64Configs)
  {
    Node::XmlNode configNode = xPathNode.node();
    std::cout << "  - Configuration: " << configNode.child("Configuration").text().get()
              << ", Platform: " << configNode.child("Platform").text().get() << "\n";
  }
  std::cout << "\n";

  // XPath 2: Select all ClCompile nodes that have the PreprocessorDefinitions element
  XPathNodeSet clCompileNodes = docRead.select_nodes("//ClCompile[PreprocessorDefinitions]");
  std::cout << "ClCompile nodes with PreprocessorDefinitions (" << clCompileNodes.size() << " found):\n";
  for(XPathNode xPathNode : clCompileNodes)
  {
    Node::XmlNode clCompileNode = xPathNode.node();
    std::cout << "  - Parent ItemDefinitionGroup Condition: " << clCompileNode.parent().attribute("Condition").value()
              << "\n";
    std::cout << "    PreprocessorDefinitions: " << clCompileNode.child("PreprocessorDefinitions").text().get() << "\n";
  }
  std::cout << "\n";

  // XPath 3: Select all 'AdditionalIncludeDirectories' values for 'Release|x64'
  XPathNodeSet releaseX64Includes
    = docRead.select_nodes("//ItemDefinitionGroup[@Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\"]/"
                           "ClCompile/AdditionalIncludeDirectories");
  std::cout << "AdditionalIncludeDirectories for Release|x64:\n";
  for(XPathNode xPathNode : releaseX64Includes) std::cout << "  - " << xPathNode.node().text().get() << "\n";
  std::cout << "\n";

  std::cout << "--- Part 4: Attribute Type Conversion and Manipulation (Hypothetical) ---\n";
  // This part demonstrates how to work with attributes as different types.
  // We'll use a dummy node for demonstration purposes as we are only reading the XML.
  Document::XmlDocument dummyDoc;
  dummyDoc.append_child("dummy").append_attribute("version").set_value(100);
  dummyDoc.document_element().append_attribute("enabled").set_value(true);
  dummyDoc.document_element().append_attribute("ratio").set_value(3.14F);

  Node::XmlNode dummyNode             = dummyDoc.document_element();

  Attribute::XmlAttribute versionAttr = dummyNode.attribute("version");
  std::cout << "Dummy Node Attributes:\n";
  std::cout << "  Version (as int): " << versionAttr.as_int() << "\n";
  versionAttr.set_value(200);
  std::cout << "  Version (after set_value): " << versionAttr.as_int() << "\n";

  Attribute::XmlAttribute enabledAttr = dummyNode.attribute("enabled");
  std::cout << "  Enabled (as bool): " << std::boolalpha << enabledAttr.as_bool() << "\n";

  Attribute::XmlAttribute ratioAttr = dummyNode.attribute("ratio");
  std::cout << "  Ratio (as float): " << ratioAttr.as_float() << "\n";
  std::cout << "\n";

  std::cout << "--- Part 5: Node and Attribute Modification (In-memory demonstration) ---\n";
  // This section demonstrates how to add/remove nodes/attributes.
  // These changes are only in memory and are not saved back to the file unless explicitly requested.

  Document::XmlDocument modifiableDoc;
  modifiableDoc.load_string(R"(<root><item id="1"/><item id="2"/></root>)");

  Node::XmlNode rootNode = modifiableDoc.document_element();
  if(rootNode != nullptr)
  {
    std::cout << "Original XML:\n";
    rootNode.print(std::cout);
    std::cout << "\n\n";

    // Append a new child node
    Node::XmlNode newItem = rootNode.append_child("item");
    newItem.append_attribute("id").set_value("3");
    newItem.append_attribute("new_attr").set_value("value");
    std::cout << "After appending new item:\n";
    rootNode.print(std::cout);
    std::cout << "\n\n";

    // Remove an attribute from an existing node
    Node::XmlNode firstItem = rootNode.child("item");
    if(firstItem != nullptr) firstItem.remove_attribute("id");
    std::cout << "After removing 'id' attribute from first item:\n";
    rootNode.print(std::cout);
    std::cout << "\n\n";

    // Remove a child node
    rootNode.remove_child(newItem);
    std::cout << "After removing the newly added item:\n";
    rootNode.print(std::cout);
    std::cout << "\n\n";
  }

  std::cout << "--- Part 6: Printing the Document to Console (Pretty Print) ---\n";
  std::cout << "Full content of '" << kXmlFilePath << "' (pretty-printed):\n";
  docRead.print(std::cout, LUMEX_XML_TEXT("  "), Constants::kformat_indent);
  std::cout << "\n";

  return 0;
}
