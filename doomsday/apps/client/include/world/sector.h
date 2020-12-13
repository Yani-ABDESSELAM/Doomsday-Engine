/** @file sector.h  Map sector.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_SECTOR_H
#define DENG_WORLD_SECTOR_H

#include <functional>
#include <QList>
#include <de/aabox.h>
#include <de/Error>
#include <de/Id>
#include <de/Observers>
#include <de/Vector>
#include <doomsday/world/MapElement>
#include "Line"
#include "Plane"

namespace world
{
    class ConvexSubspace;
    class Subsector;
}
struct mobj_s;

/**
 * World map sector.
 *
 * Whenever the @em Floor or @em Ceiling Plane moves any SoundEmitter origins which are
 * dependent on the height of said plane are updated automatically. Also, any Surfaces
 * which are missing Materials are re-evaluated (to fill any new gaps).
 */
class Sector : public world::MapElement
{
    DENG2_NO_COPY  (Sector)
    DENG2_NO_ASSIGN(Sector)

public:
    // Plane identifiers:
    enum { Floor, Ceiling };

    static de::String planeIdAsText(de::dint planeId);

    /**
     * Construct a new sector.
     *
     * @param lightLevel  Ambient light level.
     * @param lightColor  Ambient light color.
     */
    Sector(de::dfloat lightLevel          = 1,
           de::Vector3f const &lightColor = de::Vector3f(1, 1, 1));

//- Lighting ----------------------------------------------------------------------------

    /// Notified whenever a light level change occurs.
    DENG2_DEFINE_AUDIENCE2(LightLevelChange, void sectorLightLevelChanged(Sector &sector))

    /// Notified whenever a light color change occurs.
    DENG2_DEFINE_AUDIENCE2(LightColorChange, void sectorLightColorChanged(Sector &sector))

    /**
     * Returns the ambient light level in the sector. The LightLevelChange audience is
     * notified whenever the light level changes.
     *
     * @see setLightLevel()
     */
    de::dfloat lightLevel() const;

    /**
     * Change the ambient light level in the sector. The LightLevelChange audience is
     * notified whenever the light level changes.
     *
     * @param newLightLevel  New ambient light level.
     *
     * @see lightLevel()
     */
    void setLightLevel(de::dfloat newLightLevel);

    /**
     * Returns the ambient light color in the sector. The LightColorChange audience is
     * notified whenever the light color changes.
     *
     * @see setLightColor()
     */
    de::Vector3f const &lightColor() const;

    /**
     * Change the ambient light color in the sector. The LightColorChange audience is
     * notified whenever the light color changes.
     *
     * @param newLightColor  New ambient light color.
     *
     * @see lightColor()
     */
    void setLightColor(de::Vector3f const &newLightColor);

//- Map objects -------------------------------------------------------------------------

    /**
     * Unlink MapObject @a mob from the sector..
     *
     * @param mob  Mobj to be unlinked.
     */
    void unlink(struct mobj_s *mob);

    /**
     * Link the mobj to the head of the list of mobjs "in" the sector. Note that mobjs in
     * this list may not actually be inside the sector. This is because the sector is
     * determined by interpreting the BSP leaf as a half-space and not a closed convex
     * subspace (@ref world::Map::link()).
     *
     * @param mob  Mobj to be linked.
     */
    void link(struct mobj_s *mob);

    /**
     * Returns the first mobj in the linked list of mobjs "in" the sector.
     */
    struct mobj_s *firstMobj() const;

//- Planes ------------------------------------------------------------------------------

    /// Required/referenced plane is missing. @ingroup errors
    DENG2_ERROR(MissingPlaneError);

    /**
     * Returns @c true if at least one Plane in the sector is sky-masked.
     *
     * @see Surface::hasSkyMaskedMaterial()
     */
    bool hasSkyMaskPlane() const;

    /**
     * Returns the total number of planes in/owned by the sector.
     */
    de::dint planeCount() const;

    /**
     * Lookup a Plane by it's sector-unique @a planeIndex.
     */
    inline Plane &plane(de::dint planeIndex)
    {
        DENG2_ASSERT(planeIndex >= 0 && planeIndex < planeCount());
        return *_lookupPlanes[planeIndex];
    }
    
    inline Plane const &plane(de::dint planeIndex) const
    {
        DENG2_ASSERT(planeIndex >= 0 && planeIndex < planeCount());
        return *_lookupPlanes[planeIndex];
    }

    /**
     * Returns the @em floor Plane of the sector.
     */
    inline Plane       &floor()         { return plane(Floor); }
    inline Plane const &floor() const   { return plane(Floor); }

    /**
     * Returns the @em ceiling Plane of the sector.
     */
    inline Plane       &ceiling()       { return plane(Ceiling); }
    inline Plane const &ceiling() const { return plane(Ceiling); }

    /**
     * Iterate Planes of the sector.
     *
     * @param callback  Function to call for each Plane.
     */
    de::LoopResult forAllPlanes(std::function<de::LoopResult (Plane &)> func);
    de::LoopResult forAllPlanes(std::function<de::LoopResult (Plane const &)> func) const;

    /**
     * Add another Plane to the sector.
     *
     * @param normal  Map space Surface normal.
     * @param height  Map space Z axis coordinate (the "height" of the plane).
     *
     * @return  The newly constructed Plane.
     */
    Plane *addPlane(de::Vector3f const &normal, de::ddouble height);

    /**
     * Sets plane linking target and conditions. This is used for render hacks where the
     * plane is drawn at a different height and appearance, usually borrowing state
     * from a neighboring sector.
     *
     * @param sectorArchiveIndex  Target sector for plane linking.
     * @param planeBits           Flags that determine what and when to link planes:
     *                            - Bits 0...1: link the floor (bit 0), ceiling (bit 1) plane
     *                            - Bits 2...3: flat bleeding in floor (bit 2) or ceiling (bit 3)
     *                            - Bits 4...5: invisible platform (bit 4) or door (bit 5)
     */
    void setVisPlaneLinks(int sectorArchiveIndex, int planeBits);

    int visPlaneLinkTargetSector() const;

    bool isVisPlaneLinked(int planeIndex) const;

    int visPlaneBits() const;

//- Subsectors --------------------------------------------------------------------------

    typedef std::function<world::Subsector * (QVector<world::ConvexSubspace *> const &)> SubsectorConstructor;

    static void setSubsectorConstructor(SubsectorConstructor func);

    /**
     * Returns the minimum bounding rectangle containing all the subsector geometries.
     * If no subsectors are defined an invalid rectangle is returned (negative dimensions).
     *
     * @see hasSubsectors()
     */
    AABoxd const &bounds() const;

    /**
     * Returns a rough approximation of the total area of all the subsector geometries.
     * If no subsectors are defined @c 0 is returned.
     *
     * @see hasSubsectors()
     */
    de::ddouble roughArea() const;

    /**
     * Returns @c true if at least one Subsector exists for the sector.
     *
     * @see subsectorCount()
     */
    bool hasSubsectors() const;

    /**
     * Returns the total number of Subsectors of the sector.
     *
     * @see hasSubsectors()
     */
    de::dint subsectorCount() const;

    world::Subsector &subsector(int index) const;

    /**
     * Iterate Subsectors of the sector.
     *
     * @param callback  Function to call for each Subsector.
     */
    de::LoopResult forAllSubsectors(const std::function<de::LoopResult (world::Subsector &)> &callback) const;

    /**
     * Generate a new Subsector from the given set of map @a subspaces.
     *
     * @return  The newly constructed Subsector (ownership retained); otherwise @c nullptr
     */
    world::Subsector *addSubsector(QVector<world::ConvexSubspace *> const &subspaces);

//- Sides -------------------------------------------------------------------------------

    /**
     * Returns the total number of Line::Sides which reference the sector.
     */
    de::dint sideCount() const;

    /**
     * Iterate Line::Sides of the sector.
     *
     * @param callback  Function to call for each Line::Side.
     */
    de::LoopResult forAllSides(std::function<de::LoopResult (Line::Side &)> func) const;

    /**
     * (Re)Build the side list for the sector.
     *
     * @note In the special case of self-referencing line, only the front side reference
     * is added to this list.
     *
     * @attention The behavior of some algorithms used in the DOOM game logic is dependant
     * upon the order of this list. For example, EV_DoFloor and EV_BuildStairs. That same
     * order is used here, for compatibility.
     *
     * Order: Original @em line index, ascending.
     */
    void buildSides();

//---------------------------------------------------------------------------------------

    /**
     * Returns the primary sound emitter for the sector. Other emitters in the sector are
     * linked to this, forming a chain which can be traversed using the 'next' pointer of
     * the emitter's thinker_t.
     */
    SoundEmitter       &soundEmitter();
    SoundEmitter const &soundEmitter() const;

    /**
     * (Re)Build the sound emitter chains for the sector. These chains are used for
     * efficiently traversing all sound emitters in the sector (e.g., when stopping all
     * sounds emitted in the sector). To be called during map load once planes and sides
     * have been initialized.
     *
     * @see addPlane(), buildSides()
     */
    void chainSoundEmitters();

    /**
     * Returns the @em validCount of the sector. Used by some legacy iteration algorithms
     * for marking sectors as processed/visited.
     *
     * @todo Refactor away.
     */
    de::dint validCount() const;

    /// @todo Refactor away.
    void setValidCount(de::dint newValidCount);

    /**
     * Register the console commands and/or variables of this module.
     */
    static void consoleRegister();

protected:
    de::dint property(world::DmuArgs &args) const;
    de::dint setProperty(world::DmuArgs const &args);

private:
    DENG2_PRIVATE(d)

    Plane **_lookupPlanes; // heavily used; visible for inline access
};

#endif  // DENG_WORLD_SECTOR_H
