/** @file materialmanifest.h Description of a logical material resource.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIALMANIFEST_H
#define LIBDENG_RESOURCE_MATERIALMANIFEST_H

#include "Material"
#include "MaterialScheme"
#include "uri.hh"
#include <de/Error>
#include <de/Observers>
#include <de/PathTree>

namespace de {

class Materials;

/**
 * Description for a would-be logical Material resource.
 *
 * Models a reference to and the associated metadata for a logical material
 * in the material resource collection.
 *
 * @see MaterialScheme, Material
 * @ingroup resource
 */
class MaterialManifest : public PathTree::Node,
                         DENG2_OBSERVES(Material, Deletion)
{
public:
    /// Required material instance is missing. @ingroup errors
    DENG2_ERROR(MissingMaterialError);

    DENG2_DEFINE_AUDIENCE(Deletion, void manifestBeingDeleted(MaterialManifest const &manifest))

    DENG2_DEFINE_AUDIENCE(MaterialDerived, void manifestMaterialDerived(MaterialManifest &manifest, Material &material))

    enum Flag
    {
        /// The manifest was automatically produced for a game/add-on resource.
        AutoGenerated,

        /// The manifest was not produced for an original game resource.
        Custom
    };
    Q_DECLARE_FLAGS(Flags, Flag)

public:
    MaterialManifest(PathTree::NodeArgs const &args);
    virtual ~MaterialManifest();

    /**
     * Derive a new logical Material instance by interpreting the manifest.
     * The first time a material is derived from the manifest, said material
     * is assigned to the manifest (ownership is assumed).
     */
    Material *derive();

    /**
     * Returns the owning scheme of the manifest.
     */
    MaterialScheme &scheme() const;

    /// Convenience method for returning the name of the owning scheme.
    inline String const &schemeName() const { return scheme().name(); }

    /**
     * Compose a URI of the form "scheme:path" for the material manifest.
     *
     * The scheme component of the URI will contain the symbolic name of
     * the scheme for the material manifest.
     *
     * The path component of the URI will contain the percent-encoded path
     * of the material manifest.
     */
    inline Uri composeUri(QChar sep = '/') const
    {
        return Uri(schemeName(), path(sep));
    }

    /**
     * Returns a textual description of the manifest.
     *
     * @return Human-friendly description the manifest.
     */
    String description(Uri::ComposeAsTextFlags uriCompositionFlags = Uri::DefaultComposeAsTextFlags) const;

    /**
     * Returns a textual description of the source of the manifest.
     *
     * @return Human-friendly description of the source of the manifest.
     */
    String sourceDescription() const;

    /// @return  Unique identifier associated with the manifest.
    materialid_t id() const;

    void setId(materialid_t newId);

    /// @c true if the manifest was automatically produced for a game/add-on resource.
    inline bool isAutoGenerated() const { return isFlagged(AutoGenerated); }

    /// @c true if the manifest was not produced for an original game resource.
    inline bool isCustom() const { return isFlagged(Custom); }

    /**
     * Returns @c true if the manifest is flagged @a flagsToTest.
     */
    inline bool isFlagged(Flags flagsToTest) const { return !!(flags() & flagsToTest); }

    /**
     * Returns the flags for the manifest.
     */
    Flags flags() const;

    /**
     * Change the manifest's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param set  @c true to set, @c false to clear.
     */
    void setFlags(Flags flagsToChange, bool set = true);

    /**
     * Returns @c true if a Material is presently associated with the manifest.
     */
    bool hasMaterial() const;

    /**
     * Returns the logical Material associated with the manifest.
     */
    Material &material() const;

    /**
     * Change the material associated with the manifest.
     *
     * @param  material  New material to associate with.
     */
    void setMaterial(Material *material);

    /// Returns a reference to the application's material collection.
    static Materials &materials();

protected:
    // Observes Material Deletion.
    void materialBeingDeleted(Material const &material);

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MaterialManifest::Flags)

} // namespace de

#endif /* LIBDENG_RESOURCE_MATERIALMANIFEST_H */
