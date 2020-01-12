/** @file mesh.cpp  Mesh Geometry Data Structure.
 *
 * @authors Copyright © 2008-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/mesh/mesh.h"
#include "doomsday/mesh/hedge.h"
#include "doomsday/mesh/face.h"
#include "doomsday/world/vertex.h"

namespace mesh {

Mesh::Element::Element(Mesh &owner)
{
    _owner = &owner;
}

Mesh &Mesh::Element::mesh() const
{
    DE_ASSERT(_owner != nullptr);
    return *_owner;
}

bool Mesh::Element::hasMapElement() const
{
    return _mapElement != nullptr;
}

world::MapElement &Mesh::Element::mapElement()
{
    if(_mapElement) return *_mapElement;
    /// @throw MissingMapElement  Attempted with no map element attributed.
    throw MissingMapElementError("Mesh::Element::mapElement", "No map element is attributed");
}

const world::MapElement &Mesh::Element::mapElement() const
{
    return const_cast<Mesh::Element *>(this)->mapElement();
}

void Mesh::Element::setMapElement(world::MapElement *newMapElement)
{
    _mapElement = newMapElement;
}

DE_PIMPL_NOREF(Mesh)
{
    Vertices vertices;  ///< All vertices in the mesh.
    HEdges hedges;    ///< All half-edges in the mesh.
    Faces faces;      ///< All faces in the mesh.
};

Mesh::Mesh() : d(new Impl)
{}

Mesh::~Mesh()
{
    clear();
}

void Mesh::clear()
{
    deleteAll(d->vertices); d->vertices.clear();
    deleteAll(d->hedges); d->hedges.clear();
    deleteAll(d->faces); d->faces.clear();
}

world::Vertex *Mesh::newVertex(const de::Vec2d &origin)
{
    auto *vtx = new world::Vertex(*this, origin);
    d->vertices.append(vtx);
    return vtx;
}

HEdge *Mesh::newHEdge(world::Vertex &vertex)
{
    auto *hedge = new HEdge(*this, vertex);
    d->hedges.append(hedge);
    return hedge;
}

Face *Mesh::newFace()
{
    auto *face = new Face(*this);
    d->faces.append(face);
    return face;
}

void Mesh::removeVertex(world::Vertex &vertex)
{
    if (d->vertices.removeOne(&vertex))
    {
        delete &vertex;
    }
}

void Mesh::removeHEdge(HEdge &hedge)
{
    if (d->hedges.removeOne(&hedge))
    {
        delete &hedge;
    }
}

void Mesh::removeFace(Face &face)
{
    if (d->faces.removeOne(&face))
    {
        delete &face;
    }
}

const Mesh::Vertices &Mesh::vertices() const
{
    return d->vertices;
}

const Mesh::Faces &Mesh::faces() const
{
    return d->faces;
}

const Mesh::HEdges &Mesh::hedges() const
{
    return d->hedges;
}

}  // namespace mesh
