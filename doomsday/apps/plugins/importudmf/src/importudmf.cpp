/** @file importudmf.cpp  Importer plugin for UDMF maps.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "importudmf.h"
#include "udmfparser.h"

#include <doomsday/filesys/lumpindex.h>
#include <de/App>
#include <de/Log>

using namespace de;

/**
 * This function will be called when Doomsday is asked to load a map that is not
 * available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map editing
 * interface to recreate the map in native format.
 */
static int importMapHook(int /*hookType*/, int /*parm*/, void *context)
{
    if (Id1MapRecognizer const *recognizer = reinterpret_cast<Id1MapRecognizer *>(context))
    {
        if (recognizer->format() == Id1MapRecognizer::UniversalFormat)
        {
            LOG_AS("importudmf");
            try
            {
                // Read the contents of the TEXTMAP lump.
                auto *src = recognizer->lumps()[Id1MapRecognizer::UDMFTextmapData];
                Block bytes(src->size());
                src->read(bytes.data(), false);

                String const source = String::fromUtf8(bytes);

                // Parse the UDMF source and use the MPE API to create the map elements.
                UDMFParser parser;
                parser.setGlobalAssignmentHandler([] (String const &ident, QVariant const &value)
                {
                    qDebug() << "Global:" << ident << value;
                });
                parser.setBlockHandler([] (String const &type, UDMFParser::Block const &block)
                {
                    qDebug() << "Block" << type << block;
                });
                parser.parse(source);
                return true;
            }
            catch (Error const &er)
            {
                LOG_MAP_ERROR("Erroring while loading UDMF: ") << er.asText();
            }
        }
    }
    return false;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
extern "C" void DP_Initialize()
{
    Plug_AddHook(HOOK_MAP_CONVERT, importMapHook);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
extern "C" char const *deng_LibraryType()
{
    return "deng-plugin/generic";
}

//DENG_DECLARE_API(Base);
DENG_DECLARE_API(F);
DENG_DECLARE_API(Map);
DENG_DECLARE_API(Material);
DENG_DECLARE_API(MPE);

DENG_API_EXCHANGE(
    //DENG_GET_API(DE_API_BASE, Base);
    DENG_GET_API(DE_API_FILE_SYSTEM, F);
    DENG_GET_API(DE_API_MAP, Map);
    DENG_GET_API(DE_API_MATERIALS, Material);
    DENG_GET_API(DE_API_MAP_EDIT, MPE);
)