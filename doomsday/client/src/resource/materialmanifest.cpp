/** @file materialmanifest.cpp Material Manifest.
 *
 * @authors Copyright � 2011-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_defs.h"
#include <de/memory.h>

#include "render/rend_main.h"
#include "resource/materials.h"
#include "uri.hh"

#include "resource/materialmanifest.h"

namespace de {

struct MaterialManifest::Instance
{
    /// Material associated with this.
    Material *material;

    /// Unique identifier.
    materialid_t id;

    /// @c true if the material is not derived from an original game resource.
    bool isCustom;

    Instance() : material(0), id(0), isCustom(false)
    {}
};

MaterialManifest::MaterialManifest(PathTree::NodeArgs const &args)
    : Node(args)
{
    d = new Instance();
}

MaterialManifest::~MaterialManifest()
{
    delete d;
}

Materials &MaterialManifest::materials()
{
    return App_Materials();
}

void MaterialManifest::setId(materialid_t id)
{
    d->id = id;
}

void MaterialManifest::setCustom(bool yes)
{
    d->isCustom = yes;
}

MaterialScheme &MaterialManifest::scheme() const
{
    LOG_AS("MaterialManifest::scheme");
    /// @todo Optimize: MaterialManifest should contain a link to the owning MaterialScheme.
    Materials::Schemes const &schemes = materials().allSchemes();
    DENG2_FOR_EACH_CONST(Materials::Schemes, i, schemes)
    {
        MaterialScheme &scheme = **i;
        if(&scheme.index() == &tree()) return scheme;
    }

    // This should never happen...
    /// @throw Error Failed to determine the scheme of the manifest.
    throw Error("MaterialManifest::scheme", String("Failed to determine scheme for manifest [%1]").arg(de::dintptr(this)));
}

String const &MaterialManifest::schemeName() const
{
    return scheme().name();
}

Uri MaterialManifest::composeUri(QChar sep) const
{
    return Uri(schemeName(), path(sep));
}

String MaterialManifest::sourceDescription() const
{
    /// @todo We should not need a material to determine this.
    if(!d->material || !d->material->isValid()) return "unknown";
    if(!isCustom()) return "game";
    if(d->material->isAutoGenerated()) return "add-on"; // Unintuitive but correct.
    return "def";
}

materialid_t MaterialManifest::id() const
{
    return d->id;
}

bool MaterialManifest::isCustom() const
{
    return d->isCustom;
}

bool MaterialManifest::hasMaterial() const
{
    return !!d->material;
}

Material &MaterialManifest::material() const
{
    if(!d->material)
    {
        /// @throw MissingMaterialError  The manifest is not presently associated with a material.
        throw MissingMaterialError("MaterialManifest::material", "Missing required material");
    }
    return *d->material;
}

void MaterialManifest::setMaterial(Material *newMaterial)
{
    if(d->material == newMaterial) return;
    d->material = newMaterial;
}

} // namespace de
