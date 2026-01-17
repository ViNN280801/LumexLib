#include <iostream>

#include "lumex/xml/LumexXml"

static constexpr char const *kXmlFilePath = "xgconsole.xml";
static constexpr char const *kXmlContent  = R"(
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="es">
    <!-- main.qml -->
    <context>
        <name>main</name>
        <message>
            <source>LumReportViewer</source>
            <translation>Visor de Informes Lum</translation>
        </message>
    </context>

    <!-- components/MenuBar/MenuBar.qml -->
    <context>
        <name>CustomMenuBar</name>
        <message>
            <source>File</source>
            <translation>Archivo</translation>
        </message>
        <message>
            <source>Open</source>
            <translation>Abrir</translation>
        </message>
        <message>
            <source>Save as</source>
            <translation>Guardar como</translation>
        </message>
        <message>
            <source>Print</source>
            <translation>Imprimir</translation>
        </message>
        <message>
            <source>Print preview</source>
            <translation>Vista previa</translation>
        </message>
        <message>
            <source>Printer settings</source>
            <translation>Configuración de impresión</translation>
        </message>
        <message>
            <source>Exit</source>
            <translation>Salir</translation>
        </message>
        <message>
            <source>View</source>
            <translation>Ver</translation>
        </message>
        <message>
            <source>Hide footer</source>
            <translation>Ocultar pie</translation>
        </message>
        <message>
            <source>Show footer</source>
            <translation>Mostrar pie</translation>
        </message>
        <message>
            <source>Show ruler</source>
            <translation>Mostrar regla</translation>
        </message>
        <message>
            <source>Hide ruler</source>
            <translation>Ocultar regla</translation>
        </message>
        <message>
            <source>Zoom in</source>
            <translation>Acercar</translation>
        </message>
        <message>
            <source>Zoom out</source>
            <translation>Alejar</translation>
        </message>
        <message>
            <source>Settings</source>
            <translation>Configuración</translation>
        </message>
        <message>
            <source>Configure default paths</source>
            <translation>Configurar rutas predeterminadas</translation>
        </message>
        <message>
            <source>Reset to defaults</source>
            <translation>Restablecer valores</translation>
        </message>
        <message>
            <source>Language</source>
            <translation>Idioma</translation>
        </message>
        <message>
            <source>Help</source>
            <translation>Ayuda</translation>
        </message>
        <message>
            <source>About program</source>
            <translation>Acerca del programa</translation>
        </message>
    </context>

    <!-- components/PathsSettingsWindow.qml -->
    <context>
        <name>PathsSettingsWindow</name>
        <message>
            <source>Paths Settings</source>
            <translation>Configuración de rutas</translation>
        </message>
        <message>
            <source>Open:</source>
            <translation>Abrir:</translation>
        </message>
        <message>
            <source>Reset</source>
            <translation>Reiniciar</translation>
        </message>
        <message>
            <source>Apply</source>
            <translation>Aplicar</translation>
        </message>
        <message>
            <source>Browse</source>
            <translation>Examinar</translation>
        </message>
        <message>
            <source>Save:</source>
            <translation>Guardar:</translation>
        </message>
        <message>
            <source>OK</source>
            <translation>Aceptar</translation>
        </message>
        <message>
            <source>Cancel</source>
            <translation>Cancelar</translation>
        </message>
    </context>

    <!-- dialogs/AboutProgram.qml -->
    <context>
        <name>AboutProgram</name>
        <message>
            <source>About program</source>
            <translation>Acerca del programa</translation>
        </message>
        <message>
            <source>Version 2.0.000</source>
            <translation>Versión 2.0.000</translation>
        </message>
        <message>
            <source>Modern report viewer</source>
            <translation>Visor de informes moderno</translation>
        </message>
        <message>
            <source>Supports HTML, PDF, images and Office documents</source>
            <translation>Admite HTML, PDF, imágenes y documentos de Office</translation>
        </message>
        <message>
            <source>Supports HTML, PDF, images</source>
            <translation>Admite HTML, PDF, imágenes</translation>
        </message>
        <message>
            <source>- Multiple formats</source>
            <translation>- Múltiples formatos</translation>
        </message>
        <message>
            <source>- Zoom</source>
            <translation>- Zoom</translation>
        </message>
        <message>
            <source>- Print documents</source>
            <translation>- Imprimir documentos</translation>
        </message>
        <message>
            <source>- Multilingual</source>
            <translation>- Multilingüe</translation>
        </message>
        <message>
            <source>- HTML, PDF and images</source>
            <translation>- HTML, PDF y imágenes</translation>
        </message>
    </context>

    <!-- dialogs/ExitDialog.qml -->
    <context>
        <name>ExitDialog</name>
        <message>
            <source>Exit</source>
            <translation>Salir</translation>
        </message>
        <message>
            <source>Are you sure you want to exit the program?</source>
            <translation>¿Está seguro que desea salir del programa?</translation>
        </message>
        <message>
            <source>Yes</source>
            <translation>Sí</translation>
        </message>
        <message>
            <source>No</source>
            <translation>No</translation>
        </message>
    </context>

    <!-- viewer/components/LoadingIndicator.qml -->
    <context>
        <name>LoadingIndicator</name>
        <message>
            <source>Loading...</source>
            <translation>Cargando...</translation>
        </message>
    </context>

    <!-- dialogs/ErrorMessage.qml -->
    <context>
        <name>ErrorMessage</name>
        <message>
            <source>Error</source>
            <translation>Error</translation>
        </message>
    </context>

    <!-- C++/Qt classes -->
    <!-- DocumentManager -->
    <context>
        <name>DocumentManager</name>
        <message>
            <source>Can't open selected file</source>
            <translation>No se puede abrir el archivo seleccionado</translation>
        </message>
        <message>
            <source>Invalid JSON, check the formatting</source>
            <translation>JSON inválido, verifique el formato</translation>
        </message>
        <message>
            <source>Failed to load image</source>
            <translation>Error al cargar la imagen</translation>
        </message>
        <message>
            <source>You selected unsupported file format</source>
            <translation>Ha seleccionado un formato de archivo no soportado</translation>
        </message>
        <message>
            <source>You tried to open a CSV document that is too large (> 2Mb). Unfortunately, this document cannot be displayed due to internal application limitations. We apologize for the inconvenience</source>
            <translation>Intentó abrir un documento CSV demasiado grande (> 2Mb). Lamentablemente, este documento no puede mostrarse debido a limitaciones internas de la aplicación. Disculpe las molestias</translation>
        </message>
        <message>
            <source>Can't find a native printer on the PC, so the document can't be printed, you need to install a printer driver (or if you want to print the document in PDF, just use "Save as" option)</source>
            <translation>No se encontró una impresora nativa en el PC, por lo que no se puede imprimir el documento. Necesita instalar un controlador de impresora (o si desea imprimir el documento en PDF, use la opción "Guardar como")</translation>
        </message>
        <message>
            <source>Failed to load document due to internal error. Please try again or check the file</source>
            <translation>Error al cargar el documento debido a un error interno. Por favor, intente nuevamente o verifique el archivo</translation>
        </message>
        <message>
            <source>LibreOffice not found! Please install it using your package manager. For example, on Astra Linux/Debian/Ubuntu: sudo apt-get install libreoffice, for Windows: https://www.libreoffice.org/download/download-libreoffice/</source>
            <translation>¡LibreOffice no encontrado! Por favor instálelo usando su gestor de paquetes. Por ejemplo, en Astra Linux/Debian/Ubuntu: sudo apt-get install libreoffice, para Windows: https://www.libreoffice.org/download/download-libreoffice/</translation>
        </message>
        <message>
            <source>Internal error: failed to load document</source>
            <translation>Error interno: no se pudo cargar el documento</translation>
        </message>
        <message>
            <source>Failed to show settings dialog</source>
            <translation>Error al mostrar el diálogo de configuración</translation>
        </message>
        <message>
            <source>Failed to save document</source>
            <translation>Error al guardar el documento</translation>
        </message>
        <message>
            <source>Failed to save DOCX file</source>
            <translation>Error al guardar el archivo DOCX</translation>
        </message>
        <message>
            <source>Failed to save ODT file</source>
            <translation>Error al guardar el archivo ODT</translation>
        </message>
        <message>
            <source>Document that is too large (> 2Mb). Unfortunately, this document cannot be displayed due to internal application limitations. We apologize for the inconvenience</source>
            <translation>El documento es demasiado grande (> 2Mb). Lamentablemente, no se puede mostrar debido a limitaciones internas de la aplicación. Disculpe las molestias.</translation>
        </message>
        <message>
            <source>Converting PDF documents to DOCX/ODT format is not supported. Please open the original document in an editable format</source>
            <translation>La conversión de documentos PDF a formato DOCX/ODT no está soportada. Por favor, abra el documento original en un formato editable</translation>
        </message>
    </context>
</TS>
)";

using namespace Lumex::Xml;
using namespace Lumex::Xml::Types;

int
main()
{
  // --- Part 1: Create and Save an XML file using LumexXml ---
  std::cout << "--- Creating and Saving XML File (LumexXml) ---\n";
  Document::XmlDocument docWrite;

  // Parse XML content from string
  XmlParseResult resultWrite = docWrite.load_string(kXmlContent);
  if(!resultWrite)
  {
    std::cerr << "Error parsing XML content for writing: " << resultWrite.description() << "\n";
    return -1;
  }

  // Save to file with explicit text mode
  if(!docWrite.save_file(kXmlFilePath, "\t", Constants::kformat_save_file_text | Constants::kformat_no_escapes,
                         encoding_utf8))
  {
    std::cerr << "Error saving XML file: " << kXmlFilePath << "\n";
    return -1;
  }
  std::cout << "XML file '" << kXmlFilePath << "' created and saved successfully.\n\n";

  // --- Part 2: Reading and Displaying All TS Content ---
  std::cout << "--- Reading and Displaying All TS Content ---\n";
  Document::XmlDocument docRead;
  XmlParseResult resultRead = docRead.load_file(kXmlFilePath);

  if(!resultRead)
  {
    std::cerr << "Error loading XML file: " << resultRead.description() << "\n";
    return -1;
  }

  // Get root TS element
  Node::XmlNode tsRoot = docRead.child("TS");
  if(!tsRoot)
  {
    std::cerr << "Root TS element not found\n";
    return -1;
  }

  // Display TS attributes
  std::cout << "TS Document Info:\n";
  std::cout << "  Version: " << tsRoot.attribute("version").value() << "\n";
  std::cout << "  Language: " << tsRoot.attribute("language").value() << "\n\n";

  // Iterate through all contexts
  for(Node::XmlNode context : tsRoot.children("context"))
  {
    Node::XmlNode nameNode = context.child("name");
    if(nameNode != nullptr)
    {
      std::cout << "Context: " << nameNode.text().get() << "\n";
      std::cout << "----------------------------------------\n";
    }

    // Iterate through all messages in this context
    for(Node::XmlNode message : context.children("message"))
    {
      Node::XmlNode sourceNode      = message.child("source");
      Node::XmlNode translationNode = message.child("translation");

      if(sourceNode && (translationNode != nullptr))
      {
        std::cout << "  Source: \"" << sourceNode.text().get() << "\"\n";
        std::cout << "  Translation: \"" << translationNode.text().get() << "\"\n";
        std::cout << "  ---\n";
      }
    }
    std::cout << "\n";
  }

  // --- Part 3: XPath Examples for TS format ---
  std::cout << "--- XPath Examples for TS format ---\n";

  // Find all messages from specific context
  XPathNodeSet menuMessages = docRead.select_nodes("//context[name='CustomMenuBar']/message");
  std::cout << "Messages from CustomMenuBar context (" << menuMessages.size() << " found):\n";
  for(XPathNode node : menuMessages)
  {
    XmlNode message     = node.node();
    XmlNode source      = message.child("source");
    XmlNode translation = message.child("translation");
    if(source && (translation != nullptr))
      std::cout << "  " << source.text().get() << " -> " << translation.text().get() << "\n";
  }
  std::cout << "\n";

  // Find specific translation by source text
  XPathNodeSet specificMessage = docRead.select_nodes("//message[source='File']/translation");
  if(!specificMessage.empty())
    std::cout << "Translation for 'File': " << specificMessage.first().node().text().get() << "\n";

  // Count total number of translations
  XPathNodeSet allTranslations = docRead.select_nodes("//translation");
  std::cout << "Total translations in file: " << allTranslations.size() << "\n";

  return 0;
}
