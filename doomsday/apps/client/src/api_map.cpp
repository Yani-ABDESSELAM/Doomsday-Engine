/** @file api_map.cpp  Doomsday Map Update API.
 *
 * @todo Throw a game-terminating exception if an illegal value is given
 * to a public API function.
 *
 * @authors Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#define DE_NO_API_MACROS_MAP

#if defined(__CLIENT__)
#include "de_base.h"
#include "api_map.h"
#include "network/net_main.h"
#include "world/maputil.h"
#include "world/p_players.h"
#include "world/plane.h"
#include "world/map.h"
#include "world/clientserverworld.h"
#include "render/rend_fakeradio.h"
#include "ConvexSubspace"
#include "Surface"
#endif

#if defined(__SERVER__)
#include "api_map.h"
#include "dd_main.h"
#include "world/p_players.h"
#include <doomsday/resource/resources.h>
#include <doomsday/world/convexsubspace.h>
#include <doomsday/world/map.h>
#include <doomsday/world/surface.h>
#include <doomsday/world/world.h>
#include <de/c_wrapper.h>
using namespace world;
#endif

#include <de/legacy/memoryzone.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/resource/mapmanifests.h>
#include <doomsday/mesh/face.h>
#include <doomsday/world/blockmap.h>
#include <doomsday/world/bspleaf.h>
#include <doomsday/world/interceptor.h>
#include <doomsday/world/linesighttest.h>
#include <doomsday/world/lineopening.h>
#include <doomsday/world/MaterialManifest>
#include <doomsday/world/Materials>
#include <doomsday/world/plane.h>
#include <doomsday/world/sector.h>
#include <doomsday/world/thinkers.h>
#include <doomsday/EntityDatabase>

using namespace de;
using world::MapElement;
using world::DmuArgs;

// Converting a public void* pointer to an internal world::MapElement.
#define IN_ELEM(p)          reinterpret_cast<world::MapElement *>(p)
#define IN_ELEM_CONST(p)    reinterpret_cast<const world::MapElement *>(p)

#undef DMU_GetType
int DMU_GetType(const void *ptr)
{
    if(!ptr) return DMU_NONE;

    const MapElement *elem = IN_ELEM_CONST(ptr);

    // Make sure it's valid.
    switch(elem->type())
    {
    case DMU_VERTEX:
    case DMU_SEGMENT:
    case DMU_LINE:
    case DMU_SIDE:
    case DMU_SECTOR:
    case DMU_SUBSPACE:
    case DMU_PLANE:
    case DMU_SURFACE:
    case DMU_MATERIAL:
    case DMU_SKY:
        return elem->type();

    default: break; // Unknown.
    }
    return DMU_NONE;
}

#undef P_AllocDummy
void *P_AllocDummy(int type, void *extraData)
{
    return Map::createDummyElement(type, extraData);
}

#undef P_IsDummy
dd_bool P_IsDummy(const void *dummy)
{
    return Map::dummyElementType(dummy) != DMU_NONE;
}

#undef P_FreeDummy
void P_FreeDummy(void *dummy)
{
    Map::destroyDummyElement(dummy);
}

#undef P_DummyExtraData
void *P_DummyExtraData(void *dummy)
{
    return Map::dummyElementExtraData(dummy);
}

#undef P_ToIndex
int P_ToIndex(const void *ptr)
{
    if(!ptr) return -1;
    if(P_IsDummy(ptr)) return -1;

    const MapElement *elem = IN_ELEM_CONST(ptr);

    switch(elem->type())
    {
    case DMU_VERTEX:
    case DMU_LINE:
    case DMU_SIDE:
    case DMU_SECTOR:
    case DMU_SUBSPACE:
    case DMU_SKY:
        return elem->indexInMap();

    case DMU_PLANE:
        return elem->as<Plane>().indexInSector();

    case DMU_MATERIAL:
        return elem->as<world::Material>().manifest().id(); // 1-based

    default:
        DE_ASSERT_FAIL("Invalid DMU type"); // Unknown/non-indexable DMU type.
        return -1;
    }
}

#undef P_ToPtr
void *P_ToPtr(int type, int index)
{
    switch(type)
    {
    case DMU_VERTEX:
        return World::get().map().vertexPtr(index);

    case DMU_LINE:
        return World::get().map().linePtr(index);

    case DMU_SIDE:
        return World::get().map().sidePtr(index);

    case DMU_SECTOR:
        return World::get().map().sectorPtr(index);

    case DMU_PLANE:
        App_FatalError(Stringf("P_ToPtr: Cannot convert %s to a ptr (sector is unknown).",
                                      DMU_Str(type)));
        return 0; // Unreachable.

    case DMU_SUBSPACE:
        return World::get().map().subspacePtr(index);

    case DMU_SKY:
        if(index != 0) return 0; // Only one sky per map, presently.
        return &World::get().map().sky();

    case DMU_MATERIAL:
        /// @note @a index is 1-based.
        if(index > 0)
            return &world::Materials::get().toMaterialManifest(index).material();
        return 0;

    default:
        App_FatalError(Stringf("P_ToPtr: unknown type %s.", DMU_Str(type)));
        return 0; // Unreachable.
    }
}

#undef P_Count
int P_Count(int type)
{
    switch(type)
    {
    case DMU_VERTEX:    return World::get().hasMap() ? World::get().map().vertexCount()   : 0;
    case DMU_LINE:      return World::get().hasMap() ? World::get().map().lineCount()     : 0;
    case DMU_SIDE:      return World::get().hasMap() ? World::get().map().sideCount()     : 0;
    case DMU_SECTOR:    return World::get().hasMap() ? World::get().map().sectorCount()   : 0;
    case DMU_SUBSPACE:  return World::get().hasMap() ? World::get().map().subspaceCount() : 0;
    case DMU_SKY:       return 1; // Only one sky per map presently.

    case DMU_MATERIAL:  return world::Materials::get().materialCount();

    default:
        /// @throw Invalid/unknown DMU element type.
        throw Error("P_Count", stringf("Unknown type %s", DMU_Str(type)));
    }
}

#undef P_Iteratep
int P_Iteratep(void *elPtr, uint prop, int (*callback) (void *p, void *ctx), void *context)
{
    MapElement *elem = IN_ELEM(elPtr);

    switch(elem->type())
    {
    case DMU_SECTOR: {
        auto &sector = elem->as<world::Sector>();
        switch(prop)
        {
        case DMU_LINE:
            return sector.forAllSides([&callback, &context] (world::LineSide &side)
            {
                return callback(&side.line(), context);
            });

        case DMU_PLANE:
            return sector.forAllPlanes([&callback, &context] (world::Plane &plane)
            {
                return callback(&plane, context);
            });

        default:
            throw Error("P_Iteratep", stringf("Property %s unknown/not vector", DMU_Str(prop)));
        }}

    case DMU_SUBSPACE:
        /// Note: this iteration method is only needed by the games' automap.
        switch(prop)
        {
        case DMU_LINE: {
            ConvexSubspace &subspace = elem->as<ConvexSubspace>();
            auto *base  = subspace.poly().hedge();
            auto *hedge = base;
            do
            {
                if(hedge->hasMapElement())
                {
                    if(int result = callback(&hedge->mapElement().as<LineSideSegment>().line(), context))
                        return result;
                }
            } while((hedge = &hedge->next()) != base);

            LoopResult result = subspace.forAllExtraMeshes([&callback, &context] (mesh::Mesh &mesh)
            {
                for (auto *hedge : mesh.hedges())
                {
                    // Is this on the back of a one-sided line?
                    if(!hedge->hasMapElement())
                        continue;

                    if (int result = callback(&hedge->mapElement().as<LineSideSegment>().line(), context))
                        return LoopResult( result );
                }
                return LoopResult(); // continue
            });
            return result; }

        default:
            throw Error("P_Iteratep", stringf("Property %s unknown/not vector", DMU_Str(prop)));
        }

    default:
        throw Error("P_Iteratep", stringf("Type %s unknown", DMU_Str(elem->type())));
    }

    return false;
}

#undef P_Callback
int P_Callback(int type, int index, int (*callback)(void *p, void *ctx), void *context)
{
    switch(type)
    {
    case DMU_VERTEX:
        if (auto *vtx = World::get().map().vertexPtr(index))
        {
            return callback(vtx, context);
        }
        break;

    case DMU_LINE:
        if (auto *li = World::get().map().linePtr(index))
        {
            return callback(li, context);
        }
        break;

    case DMU_SIDE:
        if (auto *si = World::get().map().sidePtr(index))
        {
            return callback(si, context);
        }
        break;

    case DMU_SUBSPACE:
        if (auto *sub = World::get().map().subspacePtr(index))
        {
            return callback(sub, context);
        }
        break;

    case DMU_SECTOR:
        if (auto *sec = World::get().map().sectorPtr(index))
        {
            return callback(sec, context);
        }
        break;

    case DMU_PLANE:
        App_FatalError(Stringf(
            "P_Callback: %s cannot be referenced by id alone (sector is unknown).", DMU_Str(type)));
        return 0; // Unreachable

    case DMU_SKY: {
        if(index == 0) // Only one sky per map presently.
        {
            return callback(&World::get().map().sky(), context);
        }
        break; }

    case DMU_MATERIAL:
        if(index > 0)
            return callback(&world::Materials::get().toMaterialManifest(materialid_t(index)).material(), context);
        break;

    case DMU_LINE_BY_TAG:
    case DMU_SECTOR_BY_TAG:
    case DMU_LINE_BY_ACT_TAG:
    case DMU_SECTOR_BY_ACT_TAG:
        App_FatalError(Stringf("P_Callback: Type %s not implemented yet.", DMU_Str(type)));
        return 0; /* Unreachable */

    default:
        App_FatalError(Stringf("P_Callback: Type %s unknown (index %i).", DMU_Str(type), index));
        return 0; /* Unreachable */
    }

    return false; // Continue iteration.
}

#undef P_Callbackp
int P_Callbackp(int type, void *elPtr, int (*callback)(void *p, void *ctx), void *context)
{
    MapElement *elem = IN_ELEM(elPtr);

    LOG_AS("P_Callbackp");

    switch(type)
    {
    case DMU_VERTEX:
    case DMU_LINE:
    case DMU_SIDE:
    case DMU_SECTOR:
    case DMU_SUBSPACE:
    case DMU_PLANE:
    case DMU_MATERIAL:
    case DMU_SKY:
        // Only do the callback if the type is the same as the object's.
        if(type == elem->type())
        {
            return callback(elem, context);
        }
#ifdef DE_DEBUG
        else
        {
            LOG_DEBUG("Type mismatch %s != %s\n") << DMU_Str(type) << DMU_Str(elem->type());
            DE_ASSERT_FAIL("Type mismatch");
        }
#endif
        break;

    default:
        App_FatalError(Stringf("P_Callbackp: Type %s unknown.", DMU_Str(elem->type())));
        return 0; /* Unreachable */
    }
    return false; // Continue iteration.
}

/**
 * Only those properties that are writable by outside parties (such as games)
 * are included here. Attempting to set a non-writable property causes a
 * fatal error.
 *
 * When a property changes, the relevant subsystems are notified of the change
 * so that they can update their state accordingly.
 */
static void setProperty(MapElement *elem, world::DmuArgs &args)
{
    using world::Sector;
    
    DE_ASSERT(elem != 0);

    /**
     * @par Algorithm
     * When setting a property, reference resolution is done hierarchically so
     * that we can update all owner's of the objects being manipulated should
     * the DMU object's Set routine suggest that a change occured (which other
     * DMU objects may wish/need to respond to).
     * <ol>
     * <li> Collect references to all current owners of the object.
     * <li> Pass the change delta on to the object.
     * <li> Object responds: @c true = update owners, ELSE @c false.
     * <li> If num collected references > 0: recurse, Object = owners[n]
     * </ol>
     */

    // Dereference where necessary. Note the order, these cascade.
    if(args.type == DMU_SECTOR)
    {
        if(args.modifiers & DMU_FLOOR_OF_SECTOR)
        {
            elem = &elem->as<Sector>().floor();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = &elem->as<Sector>().ceiling();
            args.type = elem->type();
        }
    }

    if(args.type == DMU_LINE)
    {
        if(args.modifiers & DMU_FRONT_OF_LINE)
        {
            elem = &elem->as<Line>().front();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_BACK_OF_LINE)
        {
            elem = &elem->as<Line>().back();
            args.type = elem->type();
        }
    }

    if(args.type == DMU_SIDE)
    {
        if(args.modifiers & DMU_TOP_OF_SIDE)
        {
            elem = &elem->as<LineSide>().top();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_MIDDLE_OF_SIDE)
        {
            elem = &elem->as<LineSide>().middle();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_BOTTOM_OF_SIDE)
        {
            elem = &elem->as<LineSide>().bottom();
            args.type = elem->type();
        }
    }

    if(args.type == DMU_PLANE)
    {
        switch(args.prop)
        {
        case DMU_MATERIAL:
        case DMU_OFFSET_X:
        case DMU_OFFSET_Y:
        case DMU_OFFSET_XY:
        case DMU_TANGENT_X:
        case DMU_TANGENT_Y:
        case DMU_TANGENT_Z:
        case DMU_TANGENT_XYZ:
        case DMU_BITANGENT_X:
        case DMU_BITANGENT_Y:
        case DMU_BITANGENT_Z:
        case DMU_BITANGENT_XYZ:
        case DMU_NORMAL_X:
        case DMU_NORMAL_Y:
        case DMU_NORMAL_Z:
        case DMU_NORMAL_XYZ:
        case DMU_COLOR:
        case DMU_COLOR_RED:
        case DMU_COLOR_GREEN:
        case DMU_COLOR_BLUE:
        case DMU_ALPHA:
        case DMU_BLENDMODE:
        case DMU_FLAGS:
            elem = &elem->as<Plane>().surface();
            args.type = elem->type();
            break;

        default: break;
        }
    }

    // Write the property value(s).
    /// @throws MapElement::WritePropertyError  If the requested property is not writable.
    elem->setProperty(args);
}

static void getProperty(const MapElement *elem, DmuArgs &args)
{
    using world::Sector;
    
    DE_ASSERT(elem != 0);

    // Dereference where necessary. Note the order, these cascade.
    if(args.type == DMU_SECTOR)
    {
        if(args.modifiers & DMU_FLOOR_OF_SECTOR)
        {
            elem = &elem->as<Sector>().floor();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = &elem->as<Sector>().ceiling();
            args.type = elem->type();
        }
    }

    if(args.type == DMU_LINE)
    {
        if(args.modifiers & DMU_FRONT_OF_LINE)
        {
            elem = &elem->as<Line>().front();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_BACK_OF_LINE)
        {
            elem = &elem->as<Line>().back();
            args.type = elem->type();
        }
    }

    if(args.type == DMU_SIDE && args.prop != DMU_EMITTER) // emitter is in Line::Side,
    {                                                     // not Surface.
        if(args.modifiers & DMU_TOP_OF_SIDE)
        {
            elem = &elem->as<LineSide>().top();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_MIDDLE_OF_SIDE)
        {
            elem = &elem->as<LineSide>().middle();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_BOTTOM_OF_SIDE)
        {
            elem = &elem->as<LineSide>().bottom();
            args.type = elem->type();
        }
    }

    if(args.type == DMU_PLANE)
    {
        switch(args.prop)
        {
        case DMU_MATERIAL:
        case DMU_OFFSET_X:
        case DMU_OFFSET_Y:
        case DMU_OFFSET_XY:
        case DMU_TANGENT_X:
        case DMU_TANGENT_Y:
        case DMU_TANGENT_Z:
        case DMU_TANGENT_XYZ:
        case DMU_BITANGENT_X:
        case DMU_BITANGENT_Y:
        case DMU_BITANGENT_Z:
        case DMU_BITANGENT_XYZ:
        case DMU_NORMAL_X:
        case DMU_NORMAL_Y:
        case DMU_NORMAL_Z:
        case DMU_NORMAL_XYZ:
        case DMU_COLOR:
        case DMU_COLOR_RED:
        case DMU_COLOR_GREEN:
        case DMU_COLOR_BLUE:
        case DMU_ALPHA:
        case DMU_BLENDMODE:
        case DMU_FLAGS:
            elem = &elem->as<Plane>().surface();
            args.type = elem->type();
            break;

        default: break;
        }
    }

    // Read the property value(s).
    /// @throws MapElement::UnknownPropertyError  If the requested property is not readable.
    elem->property(args);

    // Currently no aggregate values are collected.
}

static int setPropertyWorker(void *elPtr, void *context)
{
    setProperty(IN_ELEM(elPtr), *reinterpret_cast<DmuArgs *>(context));
    return false; // Continue iteration.
}

#undef P_SetBool
void P_SetBool(int type, int index, uint prop, dd_bool param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetByte
void P_SetByte(int type, int index, uint prop, byte param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetInt
void P_SetInt(int type, int index, uint prop, int param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetFixed
void P_SetFixed(int type, int index, uint prop, fixed_t param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetAngle
void P_SetAngle(int type, int index, uint prop, angle_t param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetFloat
void P_SetFloat(int type, int index, uint prop, float param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetDouble
void P_SetDouble(int type, int index, uint prop, double param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = &param;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetPtr
void P_SetPtr(int type, int index, uint prop, void *param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetBoolv
void P_SetBoolv(int type, int index, uint prop, dd_bool *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetBytev
void P_SetBytev(int type, int index, uint prop, byte *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetIntv
void P_SetIntv(int type, int index, uint prop, int *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetFixedv
void P_SetFixedv(int type, int index, uint prop, fixed_t *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetAnglev
void P_SetAnglev(int type, int index, uint prop, angle_t *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetFloatv
void P_SetFloatv(int type, int index, uint prop, float *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetDoublev
void P_SetDoublev(int type, int index, uint prop, double *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = params;
    P_Callback(type, index, setPropertyWorker, &args);
}

#undef P_SetPtrv
void P_SetPtrv(int type, int index, uint prop, void *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = (void **)params;
    P_Callback(type, index, setPropertyWorker, &args);
}

/* pointer-based write functions */

#undef P_SetBoolp
void P_SetBoolp(void *ptr, uint prop, dd_bool param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetBytep
void P_SetBytep(void *ptr, uint prop, byte param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetIntp
void P_SetIntp(void *ptr, uint prop, int param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetFixedp
void P_SetFixedp(void *ptr, uint prop, fixed_t param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetAnglep
void P_SetAnglep(void *ptr, uint prop, angle_t param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetFloatp
void P_SetFloatp(void *ptr, uint prop, float param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetDoublep
void P_SetDoublep(void *ptr, uint prop, double param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = &param;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetPtrp
void P_SetPtrp(void *ptr, uint prop, void *param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetBoolpv
void P_SetBoolpv(void *ptr, uint prop, dd_bool *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetBytepv
void P_SetBytepv(void *ptr, uint prop, byte *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetIntpv
void P_SetIntpv(void *ptr, uint prop, int *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetFixedpv
void P_SetFixedpv(void *ptr, uint prop, fixed_t *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetAnglepv
void P_SetAnglepv(void *ptr, uint prop, angle_t *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetFloatpv
void P_SetFloatpv(void *ptr, uint prop, float *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetDoublepv
void P_SetDoublepv(void *ptr, uint prop, double *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = params;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

#undef P_SetPtrpv
void P_SetPtrpv(void *ptr, uint prop, void *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = (void **)params;
    P_Callbackp(args.type, ptr, setPropertyWorker, &args);
}

static int getPropertyWorker(void *elPtr, void *context)
{
    getProperty(IN_ELEM_CONST(elPtr), *reinterpret_cast<DmuArgs *>(context));
    return false; // Continue iteration.
}

/* index-based read functions */

#undef P_GetBool
dd_bool P_GetBool(int type, int index, uint prop)
{
    dd_bool returnValue = false;
    DmuArgs args(type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = &returnValue;
    P_Callback(type, index, getPropertyWorker, &args);
    return returnValue;
}

#undef P_GetByte
byte P_GetByte(int type, int index, uint prop)
{
    byte returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &returnValue;
    P_Callback(type, index, getPropertyWorker, &args);
    return returnValue;
}

#undef P_GetInt
int P_GetInt(int type, int index, uint prop)
{
    int returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &returnValue;
    P_Callback(type, index, getPropertyWorker, &args);
    return returnValue;
}

#undef P_GetFixed
fixed_t P_GetFixed(int type, int index, uint prop)
{
    fixed_t returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &returnValue;
    P_Callback(type, index, getPropertyWorker, &args);
    return returnValue;
}

#undef P_GetAngle
angle_t P_GetAngle(int type, int index, uint prop)
{
    angle_t returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &returnValue;
    P_Callback(type, index, getPropertyWorker, &args);
    return returnValue;
}

#undef P_GetFloat
float P_GetFloat(int type, int index, uint prop)
{
    float returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &returnValue;
    P_Callback(type, index, getPropertyWorker, &args);
    return returnValue;
}

#undef P_GetDouble
double P_GetDouble(int type, int index, uint prop)
{
    double returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = &returnValue;
    P_Callback(type, index, getPropertyWorker, &args);
    return returnValue;
}

#undef P_GetPtr
void *P_GetPtr(int type, int index, uint prop)
{
    void *returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &returnValue;
    P_Callback(type, index, getPropertyWorker, &args);
    return returnValue;
}

#undef P_GetBoolv
void P_GetBoolv(int type, int index, uint prop, dd_bool *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, getPropertyWorker, &args);
}

#undef P_GetBytev
void P_GetBytev(int type, int index, uint prop, byte *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, getPropertyWorker, &args);
}

#undef P_GetIntv
void P_GetIntv(int type, int index, uint prop, int *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, getPropertyWorker, &args);
}

#undef P_GetFixedv
void P_GetFixedv(int type, int index, uint prop, fixed_t *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, getPropertyWorker, &args);
}

#undef P_GetAnglev
void P_GetAnglev(int type, int index, uint prop, angle_t *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, getPropertyWorker, &args);
}

#undef P_GetFloatv
void P_GetFloatv(int type, int index, uint prop, float *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, getPropertyWorker, &args);
}

#undef P_GetDoublev
void P_GetDoublev(int type, int index, uint prop, double *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = params;
    P_Callback(type, index, getPropertyWorker, &args);
}

#undef P_GetPtrv
void P_GetPtrv(int type, int index, uint prop, void *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = (void **)params;
    P_Callback(type, index, getPropertyWorker, &args);
}

/* pointer-based read functions */

#undef P_GetBoolp
dd_bool P_GetBoolp(void *ptr, uint prop)
{
    dd_bool returnValue = false;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = &returnValue;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }

    return returnValue;
}

#undef P_GetBytep
byte P_GetBytep(void *ptr, uint prop)
{
    byte returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = &returnValue;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }

    return returnValue;
}

#undef P_GetIntp
int P_GetIntp(void *ptr, uint prop)
{
    int returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = &returnValue;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }

    return returnValue;
}

#undef P_GetFixedp
fixed_t P_GetFixedp(void *ptr, uint prop)
{
    fixed_t returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = &returnValue;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }

    return returnValue;
}

#undef P_GetAnglep
angle_t P_GetAnglep(void *ptr, uint prop)
{
    angle_t returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = &returnValue;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }

    return returnValue;
}

#undef P_GetFloatp
float P_GetFloatp(void *ptr, uint prop)
{
    float returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = &returnValue;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }

    return returnValue;
}

#undef P_GetDoublep
double P_GetDoublep(void *ptr, uint prop)
{
    double returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_DOUBLE;
        args.doubleValues = &returnValue;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }

    return returnValue;
}

#undef P_GetPtrp
void *P_GetPtrp(void *ptr, uint prop)
{
    void *returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = &returnValue;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }

    return returnValue;
}

#undef P_GetBoolpv
void P_GetBoolpv(void *ptr, uint prop, dd_bool *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = params;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }
}

#undef P_GetBytepv
void P_GetBytepv(void *ptr, uint prop, byte *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = params;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }
}

#undef P_GetIntpv
void P_GetIntpv(void *ptr, uint prop, int *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = params;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }
}

#undef P_GetFixedpv
void P_GetFixedpv(void *ptr, uint prop, fixed_t *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = params;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }
}

#undef P_GetAnglepv
void P_GetAnglepv(void *ptr, uint prop, angle_t *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = params;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }
}

#undef P_GetFloatpv
void P_GetFloatpv(void *ptr, uint prop, float *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = params;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }
}

#undef P_GetDoublepv
void P_GetDoublepv(void *ptr, uint prop, double *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_DOUBLE;
        args.doubleValues = params;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }
}

#undef P_GetPtrpv
void P_GetPtrpv(void *ptr, uint prop, void *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = (void **)params;
        P_Callbackp(args.type, ptr, getPropertyWorker, &args);
    }
}

#undef P_MapExists
DE_EXTERN_C dd_bool P_MapExists(const char *uriCString)
{
    if(!uriCString || !uriCString[0]) return false;
    return Resources::get().mapManifests().tryFindMapManifest(res::makeUri(uriCString)) != nullptr;
}

#undef P_MapIsCustom
DE_EXTERN_C dd_bool P_MapIsCustom(const char *uriCString)
{
    if(!uriCString || !uriCString[0]) return false;
    if(const res::MapManifest *mapDef = Resources::get().mapManifests().tryFindMapManifest(res::makeUri(uriCString)))
    {
        return mapDef->sourceFile()->hasCustom();
    }
    return false;
}

#undef P_MapSourceFile
DE_EXTERN_C AutoStr *P_MapSourceFile(const char *uriCString)
{
    if(!uriCString || !uriCString[0]) return nullptr;
    if(const res::MapManifest *mapDef = Resources::get().mapManifests().tryFindMapManifest(res::makeUri(uriCString)))
    {
        return AutoStr_FromTextStd(mapDef->sourceFile()->composePath());
    }
    return AutoStr_NewStd();
}

#undef P_MapChange
DE_EXTERN_C dd_bool P_MapChange(const char *uriCString)
{
    if (!uriCString || !uriCString[0])
    {
        App_FatalError("P_MapChange: Invalid Uri argument.");
    }

    // Initialize the logical sound manager.
    App_AudioSystem().aboutToUnloadMap();

#ifdef __CLIENT__
    App_Resources().purgeCacheQueue();
#endif

#ifdef __SERVER__
    // Whenever the map changes, remote players must tell us when they're
    // ready to begin receiving frames.
    for (uint i = 0; i < DDMAXPLAYERS; ++i)
    {
        if (DD_Player(i)->isConnected())
        {
            LOG_DEBUG("Client %i marked as 'not ready' to receive frames.") << i;
            DD_Player(i)->ready = false;
        }
    }
#endif

    return (dd_bool) App_World().changeMap(res::makeUri(uriCString));
}

#undef P_CountMapObjs
DE_EXTERN_C uint P_CountMapObjs(int entityId)
{
    if(!App_World().hasMap()) return 0;
    EntityDatabase &entities = World::get().map().entityDatabase();
    return entities.entityCount(P_MapEntityDef(entityId));
}

#undef Mobj_Link
DE_EXTERN_C void Mobj_Link(mobj_t *mobj, int flags)
{
    if(!mobj || !App_World().hasMap()) return; // Huh?
    World::get().map().link(*mobj, flags);
}

#undef Mobj_Unlink
DE_EXTERN_C void Mobj_Unlink(mobj_t *mobj)
{
    if(!mobj || !Mobj_IsLinked(*mobj)) return;
    Mobj_Map(*mobj).unlink(*mobj);
}

#undef Mobj_TouchedLinesIterator
DE_EXTERN_C int Mobj_TouchedLinesIterator(mobj_t *mob, int (*callback) (world::Line *, void *), void *context)
{
    DE_ASSERT(mob && callback);
    LoopResult result = Mobj_Map(*mob).forAllLinesTouchingMobj(*mob, [&callback, &context] (world::Line &line)
    {
        return LoopResult( callback(&line, context) );
    });
    return result;
}

#undef Mobj_TouchedSectorsIterator
DE_EXTERN_C int Mobj_TouchedSectorsIterator(mobj_t *mob, int (*callback) (world::Sector *, void *), void *context)
{
    DE_ASSERT(mob && callback);
    LoopResult result = Mobj_Map(*mob).forAllSectorsTouchingMobj(*mob, [&callback, &context] (world::Sector &sector)
    {
        return LoopResult( callback(&sector, context) );
    });
    return result;
}

#undef Line_TouchingMobjsIterator
DE_EXTERN_C int Line_TouchingMobjsIterator(world_Line *line, int (*callback) (mobj_t *, void *), void *context)
{
    DE_ASSERT(line && callback);
    LoopResult result = line->map().forAllMobjsTouchingLine(*line, [&callback, &context] (mobj_t &mob)
    {
        return LoopResult( callback(&mob, context) );
    });
    return result;
}

#undef Sector_TouchingMobjsIterator
DE_EXTERN_C int Sector_TouchingMobjsIterator(world_Sector *sector, int (*callback) (mobj_t *, void *), void *context)
{
    DE_ASSERT(sector && callback);
    LoopResult result = sector->map().forAllMobjsTouchingSector(*sector, [&callback, &context] (mobj_t &mob)
    {
        return LoopResult( callback(&mob, context) );
    });
    return result;
}

#undef Sector_AtPoint_FixedPrecision
DE_EXTERN_C world_Sector *Sector_AtPoint_FixedPrecision(const_pvec2d_t point)
{
    if(!App_World().hasMap()) return 0;
    return World::get().map().bspLeafAt_FixedPrecision(Vec2d(point)).sectorPtr();
}

#undef Mobj_BoxIterator
DE_EXTERN_C int Mobj_BoxIterator(const AABoxd *box,
    int (*callback) (mobj_t *, void *), void *context)
{
    DE_ASSERT(box && callback);

    LoopResult result = LoopContinue;
    if(App_World().hasMap())
    {
        const auto &map           = World::get().map();
        const int localValidCount = World::validCount;

        result = map.mobjBlockmap().forAllInBox(*box, [&callback, &context, &localValidCount] (void *object)
        {
            mobj_t &mob = *reinterpret_cast<mobj_t *>(object);
            if(mob.validCount != localValidCount) // not yet processed
            {
                mob.validCount = localValidCount;
                return LoopResult( callback(&mob, context) );
            }
            return LoopResult(); // continue
        });
    }
    return result;
}

#undef Polyobj_BoxIterator
DE_EXTERN_C int Polyobj_BoxIterator(const AABoxd *box,
    int (*callback) (struct polyobj_s *, void *), void *context)
{
    DE_ASSERT(box && callback);

    LoopResult result = LoopContinue;
    if(App_World().hasMap())
    {
        const auto &map            = World::get().map();
        const dint localValidCount = World::validCount;

        result = map.polyobjBlockmap().forAllInBox(*box, [&callback, &context, &localValidCount] (void *object)
        {
            auto &pob = *reinterpret_cast<Polyobj *>(object);
            if(pob.validCount != localValidCount) // not yet processed
            {
                pob.validCount = localValidCount;
                return LoopResult( callback(&pob, context) );
            }
            return LoopResult(); // continue
        });
    }
    return result;
}

#undef Line_BoxIterator
DE_EXTERN_C int Line_BoxIterator(const AABoxd *box, int flags,
    int (*callback) (world_Line *, void *), void *context)
{
    DE_ASSERT(box && callback);
    if(!App_World().hasMap()) return LoopContinue;

    return World::get().map().forAllLinesInBox(*box, flags, [&callback, &context] (world::Line &line)
    {
        return LoopResult( callback(&line, context) );
    });
}

#undef Subspace_BoxIterator
DE_EXTERN_C int Subspace_BoxIterator(const AABoxd *box,
    int (*callback) (struct convexsubspace_s *, void *), void *context)
{
    DE_ASSERT(box && callback);
    if (!App_World().hasMap()) return LoopContinue;

    const dint localValidCount = World::validCount;

    return World::get().map().subspaceBlockmap()
        .forAllInBox(*box, [&box, &callback, &context, &localValidCount] (void *object)
    {
        auto &sub = *(ConvexSubspace *)object;
        if (sub.validCount() != localValidCount) // not yet processed
        {
            sub.setValidCount(localValidCount);
            // Check the bounds.
            const AABoxd &polyBounds = sub.poly().bounds();
            if (!(   polyBounds.maxX < box->minX
                  || polyBounds.minX > box->maxX
                  || polyBounds.minY > box->maxY
                  || polyBounds.maxY < box->minY))
            {
                return LoopResult( callback((convexsubspace_s *) &sub, context) );
            }
        }
        return LoopResult(); // continue
    });
}

#undef P_PathTraverse2
DE_EXTERN_C int P_PathTraverse2(const_pvec2d_t from, const_pvec2d_t to,
    int flags, traverser_t callback, void *context)
{
    if(!App_World().hasMap()) return false;  // Continue iteration.

    return world::Interceptor(callback, Vec2d(from), Vec2d(to), flags, context)
                .trace(World::get().map());
}

#undef P_PathTraverse
DE_EXTERN_C int P_PathTraverse(const_pvec2d_t from, const_pvec2d_t to,
    traverser_t callback, void *context)
{
    if(!App_World().hasMap()) return false;  // Continue iteration.

    return world::Interceptor(callback, Vec2d(from), Vec2d(to), PTF_ALL, context)
                .trace(World::get().map());
}

#undef P_CheckLineSight
DE_EXTERN_C dd_bool P_CheckLineSight(const_pvec3d_t from, const_pvec3d_t to, coord_t bottomSlope,
    coord_t topSlope, int flags)
{
    if(!App_World().hasMap()) return false;  // Continue iteration.

    return world::LineSightTest(Vec3d(from), Vec3d(to), bottomSlope, topSlope, flags)
                .trace(World::get().map().bspTree());
}

#undef Interceptor_Origin
DE_EXTERN_C const coord_t *Interceptor_Origin(const world_Interceptor *trace)
{
    if(!trace) return 0;
    return trace->origin();
}

#undef Interceptor_Direction
DE_EXTERN_C const coord_t *(Interceptor_Direction)(const world_Interceptor *trace)
{
    if(!trace) return 0;
    return trace->direction();
}

#undef Interceptor_Opening
DE_EXTERN_C const LineOpening *Interceptor_Opening(const world_Interceptor *trace)
{
    if(!trace) return 0;
    return &trace->opening();
}

#undef Interceptor_AdjustOpening
DE_EXTERN_C dd_bool Interceptor_AdjustOpening(world_Interceptor *trace, world_Line *line)
{
    if(!trace) return false;
    return trace->adjustOpening(line);
}

#undef Mobj_CreateXYZ
DE_EXTERN_C mobj_t *Mobj_CreateXYZ(thinkfunc_t function, coord_t x, coord_t y, coord_t z,
    angle_t angle, coord_t radius, coord_t height, int ddflags)
{
    return P_MobjCreate(function, Vec3d(x, y, z), angle, radius, height, ddflags);
}

// p_mobj.c
DE_EXTERN_C void Mobj_Destroy(mobj_t *mobj);
DE_EXTERN_C void Mobj_SetState(mobj_t *mobj, int statenum);
DE_EXTERN_C angle_t Mobj_AngleSmoothed(mobj_t *mobj);
DE_EXTERN_C void Mobj_OriginSmoothed(mobj_t *mobj, coord_t origin[3]);
DE_EXTERN_C world_Sector *Mobj_Sector(const mobj_t *mobj);
DE_EXTERN_C void Mobj_SpawnDamageParticleGen(const mobj_t *mobj, const mobj_t *inflictor, int amount);

#undef Polyobj_SetCallback
DE_EXTERN_C void Polyobj_SetCallback(void (*func) (struct mobj_s *, void *, void *))
{
    Polyobj::setCollisionCallback(func);
}

#undef Polyobj_Unlink
DE_EXTERN_C void Polyobj_Unlink(Polyobj *po)
{
    if(!po) return;
    po->unlink();
}

#undef Polyobj_Link
DE_EXTERN_C void Polyobj_Link(Polyobj *po)
{
    if(!po) return;
    po->link();
}

#undef Polyobj_ById
DE_EXTERN_C Polyobj *Polyobj_ById(int index)
{
    if(!App_World().hasMap()) return nullptr;
    return World::get().map().polyobjPtr(index);
}

#undef Polyobj_ByTag
DE_EXTERN_C Polyobj *Polyobj_ByTag(int tag)
{
    Polyobj *found = nullptr; // not found.
    if(App_World().hasMap())
    {
        World::get().map().forAllPolyobjs([&tag, &found] (Polyobj &pob)
        {
            if(pob.tag == tag)
            {
                found = &pob;
                return LoopAbort;
            }
            return LoopContinue;
        });
    }
    return found;
}

#undef Polyobj_Move
DE_EXTERN_C dd_bool Polyobj_Move(Polyobj *po, const_pvec3d_t xy)
{
    if(!po) return false;
    return po->move(Vec3d(xy));
}

#undef Polyobj_MoveXY
DE_EXTERN_C dd_bool Polyobj_MoveXY(Polyobj *po, coord_t x, coord_t y)
{
    if(!po) return false;
    return po->move(x, y);
}

#undef Polyobj_Rotate
DE_EXTERN_C dd_bool Polyobj_Rotate(Polyobj *po, angle_t angle)
{
    if(!po) return false;
    return po->rotate(angle);
}

#undef Polyobj_FirstLine
DE_EXTERN_C world_Line *Polyobj_FirstLine(Polyobj *po)
{
    if(!po) return 0;
    return po->lines()[0];
}

#undef Line_PointDistance
DE_EXTERN_C coord_t Line_PointDistance(world_Line *line, const coord_t point[2], coord_t *offset)
{
    DE_ASSERT(line);
    return line->pointDistance(Vec2d(point), offset);
}

#undef Line_PointOnSide
DE_EXTERN_C coord_t Line_PointOnSide(const world_Line *line, const coord_t point[2])
{
    DE_ASSERT(line);
    if(!point)
    {
        LOG_AS("Line_PointOnSide");
        LOG_DEBUG("Invalid arguments, returning >0.");
        return 1;
    }
    return line->pointOnSide(Vec2d(point));
}

#undef Line_BoxOnSide
DE_EXTERN_C int Line_BoxOnSide(world_Line *line, const AABoxd *box)
{
    DE_ASSERT(line && box);
    return line->boxOnSide(*box);
}

#undef Line_BoxOnSide_FixedPrecision
DE_EXTERN_C int Line_BoxOnSide_FixedPrecision(world_Line *line, const AABoxd *box)
{
    DE_ASSERT(line && box);
    return line->boxOnSide_FixedPrecision(*box);
}

#undef Line_Opening
DE_EXTERN_C void Line_Opening(world_Line *line, LineOpening *opening)
{
    DE_ASSERT(line && opening);
    *opening = LineOpening(*line);
}

/*
 * Locates a mobj by it's unique identifier in the CURRENT map.
 */
#undef Mobj_ById
DE_EXTERN_C struct mobj_s *Mobj_ById(dint id)
{
    /// @todo fixme: Do not assume the current map.
    if (!World::get().hasMap()) return nullptr;
    return World::get().map().thinkers().mobjById(id);
}

DE_DECLARE_API(Map) =
{
    { DE_API_MAP },
    P_MapExists,
    P_MapIsCustom,
    P_MapSourceFile,
    P_MapChange,

    Line_BoxIterator,
    Line_BoxOnSide,
    Line_BoxOnSide_FixedPrecision,
    Line_PointDistance,
    Line_PointOnSide,
    Line_TouchingMobjsIterator,
    Line_Opening,

    Sector_TouchingMobjsIterator,
    Sector_AtPoint_FixedPrecision,

    Mobj_CreateXYZ,
    Mobj_Destroy,
    Mobj_ById,
    Mobj_BoxIterator,
    Mobj_SetState,
    Mobj_Link,
    Mobj_Unlink,
    Mobj_SpawnDamageParticleGen,
    Mobj_TouchedLinesIterator,
    Mobj_TouchedSectorsIterator,
    Mobj_OriginSmoothed,
    Mobj_AngleSmoothed,

    Polyobj_MoveXY,
    Polyobj_Rotate,
    Polyobj_Link,
    Polyobj_Unlink,
    Polyobj_FirstLine,
    Polyobj_ById,
    Polyobj_ByTag,
    Polyobj_BoxIterator,
    Polyobj_SetCallback,

    Subspace_BoxIterator,

    P_PathTraverse,
    P_PathTraverse2,
    P_CheckLineSight,

    Interceptor_Origin,
    Interceptor_Direction,
    Interceptor_Opening,
    Interceptor_AdjustOpening,

    DMU_Str,
    DMU_GetType,
    P_ToIndex,
    P_ToPtr,
    P_Count,
    P_Callback,
    P_Callbackp,
    P_Iteratep,
    P_AllocDummy,
    P_FreeDummy,
    P_IsDummy,
    P_DummyExtraData,
    P_CountMapObjs,
    P_SetBool,
    P_SetByte,
    P_SetInt,
    P_SetFixed,
    P_SetAngle,
    P_SetFloat,
    P_SetDouble,
    P_SetPtr,
    P_SetBoolv,
    P_SetBytev,
    P_SetIntv,
    P_SetFixedv,
    P_SetAnglev,
    P_SetFloatv,
    P_SetDoublev,
    P_SetPtrv,
    P_SetBoolp,
    P_SetBytep,
    P_SetIntp,
    P_SetFixedp,
    P_SetAnglep,
    P_SetFloatp,
    P_SetDoublep,
    P_SetPtrp,
    P_SetBoolpv,
    P_SetBytepv,
    P_SetIntpv,
    P_SetFixedpv,
    P_SetAnglepv,
    P_SetFloatpv,
    P_SetDoublepv,
    P_SetPtrpv,
    P_GetBool,
    P_GetByte,
    P_GetInt,
    P_GetFixed,
    P_GetAngle,
    P_GetFloat,
    P_GetDouble,
    P_GetPtr,
    P_GetBoolv,
    P_GetBytev,
    P_GetIntv,
    P_GetFixedv,
    P_GetAnglev,
    P_GetFloatv,
    P_GetDoublev,
    P_GetPtrv,
    P_GetBoolp,
    P_GetBytep,
    P_GetIntp,
    P_GetFixedp,
    P_GetAnglep,
    P_GetFloatp,
    P_GetDoublep,
    P_GetPtrp,
    P_GetBoolpv,
    P_GetBytepv,
    P_GetIntpv,
    P_GetFixedpv,
    P_GetAnglepv,
    P_GetFloatpv,
    P_GetDoublepv,
    P_GetPtrpv
};
