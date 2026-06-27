//----------------------------------------------------------------------------------------------------
// BlockDefinition.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Definition/BlockDefinition.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

//----------------------------------------------------------------------------------------------------
STATIC std::vector<sBlockDefinition*> sBlockDefinition::s_definitions;

//----------------------------------------------------------------------------------------------------
sBlockDefinition::~sBlockDefinition()
{
}

//----------------------------------------------------------------------------------------------------
bool sBlockDefinition::LoadFromXmlElement(XmlElement const* element)
{
    m_name               = ParseXmlAttribute(*element, "name", "DEFAULT");
    m_isVisible          = ParseXmlAttribute(*element, "isVisible", false);
    m_isSolid            = ParseXmlAttribute(*element, "isSolid", false);
    m_isOpaque           = ParseXmlAttribute(*element, "isOpaque", false);
    m_topSpriteCoords    = ParseXmlAttribute(*element, "topSpriteCoords", IntVec2::ZERO);
    m_bottomSpriteCoords = ParseXmlAttribute(*element, "bottomSpriteCoords", IntVec2::ZERO);
    m_sideSpriteCoords   = ParseXmlAttribute(*element, "sideSpriteCoords", IntVec2::ZERO);
    m_indoorLighting     = ParseXmlAttribute(*element, "indoorLighting", 0.f);

    return true;
}

//----------------------------------------------------------------------------------------------------
void sBlockDefinition::InitializeDefinitionFromFile(char const* path)
{
    XmlDocument     document;
    XmlResult const result = document.LoadFile(path);

    if (result != XmlResult::XML_SUCCESS)
    {
        ERROR_AND_DIE("Failed to load XML file")
    }

    XmlElement const* rootElement = document.RootElement();

    if (rootElement == nullptr)
    {
        ERROR_AND_DIE("XML file %s is missing a root element.")
    }

    XmlElement const* blockDefinitionElement = rootElement->FirstChildElement();

    while (blockDefinitionElement != nullptr)
    {
        String            elementName     = blockDefinitionElement->Name();
        sBlockDefinition* blockDefinition = new sBlockDefinition();

        if (blockDefinition->LoadFromXmlElement(blockDefinitionElement))
        {
            s_definitions.push_back(blockDefinition);
        }
        else
        {
            delete &blockDefinition;
            ERROR_AND_DIE("Failed to load piece definition")
        }

        blockDefinitionElement = blockDefinitionElement->NextSiblingElement();
    }
}

//----------------------------------------------------------------------------------------------------
sBlockDefinition* sBlockDefinition::GetDefinitionByIndex(uint8_t typeIndex)
{
    if (typeIndex >= s_definitions.size())
    {
        return nullptr;
    }
    
    return s_definitions[typeIndex];
}
