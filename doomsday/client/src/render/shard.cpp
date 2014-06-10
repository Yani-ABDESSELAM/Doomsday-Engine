/** @file shard.cpp  3D map geometry shard.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "render/shard.h"

using namespace de;

Vector2f Shard::Primitive::texOffset(int unit) const
{
    DENG2_ASSERT(unit >= 0 && unit < 2);
    if(texunits[unit].useOffset)
    {
        return texunits[unit].offset;
    }
    return Vector2f();
}

Vector2f Shard::Primitive::texScale(int unit) const
{
    DENG2_ASSERT(unit >= 0 && unit < 2);
    if(texunits[unit].useScale)
    {
        return texunits[unit].scale;
    }
    return Vector2f(1, 1);
}

Shard::Primitive &Shard::Primitive::setTexOffset(int unit, Vector2f const &newOffset)
{
    DENG2_ASSERT(unit >= 0 && unit < 2);
    texunits[unit].offset    = newOffset;
    texunits[unit].useOffset = true;
    return *this;
}

Shard::Primitive &Shard::Primitive::setTexScale(int unit, Vector2f const &newScale)
{
    DENG2_ASSERT(unit >= 0 && unit < 2);
    texunits[unit].scale    = newScale;
    texunits[unit].useScale = true;
    return *this;
}

Shard::Shard(Type type, blendmode_t blendmode,
    GLuint modTex, Vector3f const &modColor, bool hasDynlights)
    : type        (type)
    , blendmode   (blendmode)
    , modTex      (modTex)
    , modColor    (modColor)
    , hasDynlights(hasDynlights)
{}

Shard &Shard::setAllTextureUnits(GLTextureUnit const *gltumap[])
{
    DENG2_ASSERT(gltumap != 0);
    for(int i = 0; i < NUM_TEXTURE_UNITS; ++i)
    {
        if(gltumap[i]) setTextureUnit(texunitid_t(i), *gltumap[i]);
    }
    return *this;
}

Shard &Shard::setTextureUnit(texunitid_t unit, GLTextureUnit const &gltu)
{
    listSpec.unit(unit) = gltu;
    return *this;
}

Shard::Primitive &Shard::newPrimitive(gl::Primitive type, WorldVBuf::Index vertCount,
    WorldVBuf &vbuf, WorldVBuf::Index indicesOffset)
{
    primitives.append(Primitive());
    Primitive &prim = primitives.last();

    prim.type      = type;
    prim.vbuffer   = &vbuf;
    prim.vertCount = vertCount;
    prim.indices   = indices.data() + indicesOffset;

    return prim;
}
