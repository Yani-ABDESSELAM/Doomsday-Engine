/** @file sector.h Map Sector.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_SECTOR
#define LIBDENG_MAP_SECTOR

#include <QList>
#include <de/aabox.h>
#include <de/vector1.h>
#include <de/Error>
#include "map/plane.h"
#include "p_mapdata.h"
#include "p_dmu.h"
#include "MapElement"

class BspLeaf;
class LineDef;

/**
 * @defgroup sectorFrameFlags Sector frame flags
 * @ingroup map
 */
///@{
#define SIF_VISIBLE             0x1 ///< Sector is visible on this frame.
#define SIF_LIGHT_CHANGED       0x2

// Flags to clear before each frame.
#define SIF_FRAME_CLEAR         SIF_VISIBLE
///@}

/**
 * Map sector.
 *
 * @ingroup map
 */
class Sector : public de::MapElement
{
public:
    /// Required/referenced plane is missing. @ingroup errors
    DENG2_ERROR(MissingPlaneError);

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    typedef QList<LineDef *> Lines;
    typedef QList<Plane *> Planes;
    typedef QList<BspLeaf *> BspLeafs;

    /**
     * LightGrid data values for "smoothed sector lighting".
     */
    struct LightGridData
    {
        /// Number of blocks attributed to the sector.
        uint blockCount;

        /// Number of attributed blocks to mark changed.
        uint changedBlockCount;

        /// Block indices.
        ushort *blocks;
    };

public: /// @todo Make private:
    /// @ref sectorFrameFlags
    int _frameFlags;

    /// if == validCount, already checked.
    int _validCount;

    /// Bounding box for the sector.
    AABoxd _aaBox;

    /// Rough approximation of sector area.
    coord_t _roughArea;

    /// Ambient light level in the sector.
    float _lightLevel;

    /// Old ambient light level in the sector. For smoothing.
    float _oldLightLevel;

    /// Ambient light color in the sector.
    vec3f_t _lightColor;

    /// Old ambient light color in the sector. For smoothing.
    vec3f_t _oldLightColor;

    /// Head of the linked list of mobjs "in" the sector (not owned).
    struct mobj_s *_mobjList;

    /// List of lines which reference the sector (not owned).
    Lines _lines;

    /// List of BSP leafs which reference the sector (not owned).
    BspLeafs _bspLeafs;

    /// List of BSP leafs which contribute to the environmental audio
    /// characteristics of the sector (not owned).
    BspLeafs _reverbBspLeafs;

    /// Primary sound emitter. Others are linked to this, forming a chain.
    ddmobj_base_t _soundEmitter;

    /// List of sector planes (owned).
    Planes _planes;

    /// LightGrid data values.
    LightGridData _lightGridData;

    /// Final environmental audio characteristics.
    AudioEnvironmentFactors _reverb;

    /// Original index in the archived map.
    int _origIndex;

public:
    Sector();    
    ~Sector();

    /**
     * Returns the ambient light level in the sector.
     */
    float lightLevel() const;

    /**
     * Returns the ambient light color in the sector.
     */
    const_pvec3f_t &lightColor() const;

    /**
     * Returns the first mobj in the linked list of mobjs "in" the sector.
     */
    struct mobj_s *firstMobj() const;

    /**
     * Returns the primary sound emitter for the sector. Other emitters in the
     * sector are linked to this, forming a chain which can be traversed using
     * the 'next' pointer of the emitter's thinker_t.
     */
    ddmobj_base_t &soundEmitter();

    /// @copydoc soundEmitter()
    ddmobj_base_t const &soundEmitter() const;

    /**
     * Returns the final environmental audio characteristics of the sector.
     */
    AudioEnvironmentFactors const &audioEnvironmentFactors() const;

    /**
     * Returns the original index of the sector.
     */
    uint origIndex() const;

    /**
     * Returns the @ref sectorFrameFlags for the sector.
     */
    int frameFlags() const;

    /**
     * Returns the @em validCount of the sector. Used by some legacy iteration
     * algorithms for marking sectors as processed/visited.
     *
     * @todo Refactor away.
     */
    int validCount() const;

    /**
     * Returns the sector plane with the specified @a planeIndex.
     */
    Plane &plane(int planeIndex);

    /// @copydoc plane()
    Plane const &plane(int planeIndex) const;

    /**
     * Returns the floor plane of the sector.
     */
    inline Plane &floor() { return plane(Plane::Floor); }

    /// @copydoc floor()
    inline Plane const &floor() const { return plane(Plane::Floor); }

    /**
     * Returns the ceiling plane of the sector.
     */
    inline Plane &ceiling() { return plane(Plane::Ceiling); }

    /// @copydoc ceiling()
    inline Plane const &ceiling() const { return plane(Plane::Ceiling); }

    /**
     * Convenient accessor method for returning the surface of the specified
     * plane of the sector.
     */
    inline Surface &planeSurface(int planeIndex) { return plane(planeIndex).surface(); }

    /// @copydoc planeSurface()
    inline Surface const &planeSurface(int planeIndex) const { return plane(planeIndex).surface(); }

    /**
     * Convenient accessor method for returning the surface of the floor plane
     * of the sector.
     */
    inline Surface &floorSurface() { return floor().surface(); }

    /// @copydoc floorSurface()
    inline Surface const &floorSurface() const { return floor().surface(); }

    /**
     * Convenient accessor method for returning the surface of the ceiling plane
     * of the sector.
     */
    inline Surface &ceilingSurface() { return ceiling().surface(); }

    /// @copydoc ceilingSurface()
    inline Surface const &ceilingSurface() const { return ceiling().surface(); }

    /**
     * Provides access to the list of lines which reference the sector, for
     * efficient traversal.
     */
    Lines const &lines() const;

    /**
     * Returns the total number of lines which reference the sector.
     */
    inline uint lineCount() const { return uint(lines().count()); }

    /**
     * Provides access to the list of planes for efficient traversal.
     */
    Planes const &planes() const;

    /**
     * Returns the total number of planes in the sector.
     */
    inline uint planeCount() const { return uint(planes().count()); }

    /**
     * Provides access to the list of BSP leafs which reference the sector, for
     * efficient traversal.
     */
    BspLeafs const &bspLeafs() const;

    /**
     * Returns the total number of BSP leafs which reference the sector.
     */
    inline uint bspLeafCount() const { return uint(bspLeafs().count()); }

    /**
     * Provides access to the list of BSP leafs which contribute to the environmental
     * audio characteristics of the sector, for efficient traversal.
     */
    BspLeafs const &reverbBspLeafs() const;

    /**
     * Returns the total number of BSP leafs which contribute to the environmental
     * audio characteristics of the sector.
     */
    inline uint reverbBspLeafCount() const { return uint(reverbBspLeafs().count()); }

    /**
     * Returns the axis-aligned bounding box which encompases all vertex
     * origin points for lines which reference the sector, in map coordinate
     * space units. Note that if no lines reference the sector the bounding
     * box will be invalid (has negative dimensions).
     *
     * @deprecated Algorithms which are dependent on this are likely making
     * invalid assumptions about the geometry of the map.
     */
    AABoxd const &aaBox() const;

    /**
     * Update the sector's map space axis-aligned bounding box to encompass
     * the points defined by it's LineDefs' vertexes.
     * @pre Line list must have been initialized.
     */
    void updateAABox();

    /**
     * Returns a rough approximation of the area of the sector in the map
     * coordinate space (units squared).
     *
     * @deprecated Algorithms which are dependent on this are likely making
     * invalid assumptions about the geometry of the map.
     */
    coord_t roughArea() const;

    /**
     * Update the sector's rough area approximation.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateRoughArea();

    /**
     * @param base  Mobj base to link in @a sector. Caller should ensure that the
     *              same object is not linked multiple times into the chain.
     */
    void linkSoundEmitter(ddmobj_base_t &newEmitter);

    /**
     * Update the origin of the sector according to the point defined by the
     * center of the sector's axis-aligned bounding box (which must be
     * initialized before calling).
     */
    void updateSoundEmitterOrigin();

    /**
     * Get a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int property(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const &args);
};

#endif // LIBDENG_MAP_SECTOR
