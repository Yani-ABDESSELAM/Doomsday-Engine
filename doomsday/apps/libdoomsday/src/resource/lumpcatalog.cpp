/** @file lumpcatalog.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/resource/lumpcatalog.h"
#include "doomsday/resource/lumpdirectory.h"
#include "doomsday/resource/databundle.h"

#include <de/App>
#include <de/LinkFile>
#include <de/PackageLoader>
#include <utility>

using namespace de;

namespace res {

DENG2_PIMPL(LumpCatalog)
{
    StringList packageIds;
    QList<const DataBundle *> bundles; /// @todo Should observe for deletion. -jk

    Impl(Public *i)
        : Base(i)
    {}
    
    Impl(Public *i, const Impl &other)
        : Base(i)
        , packageIds(other.packageIds)
        , bundles(other.bundles)
    {}

    void clear()
    {
        packageIds.clear();
        bundles.clear();
    }

    void updateBundles()
    {
        bundles.clear();

        foreach (const auto &pkg, packageIds)
        {
            // The package must be available as a file.
            if (File const *file = App::packageLoader().select(pkg))
            {
                auto const *bundle = maybeAs<DataBundle>(file->target());
                if (bundle && bundle->lumpDirectory())
                {
                    bundles << bundle;
                }
            }
        }
    }

    LumpPos findLump(const String &name) const
    {
        Block const lumpName = name.toLatin1();
        LumpPos found { nullptr, LumpDirectory::InvalidPos };

        // The last bundle is checked first.
        for (int i = bundles.size() - 1; i >= 0; --i)
        {
            if (const auto *bundle = maybeAs<DataBundle>(bundles.at(i)))
            {
                auto const pos = bundle->lumpDirectory()->find(lumpName);
                if (pos != LumpDirectory::InvalidPos)
                {
                    found = LumpPos(bundle, pos);
                    break;
                }
            }
            else
            {
                qDebug() << "LumpCatalog is outdated: a bundle has been deleted";
            }
        }
        return found;
    }
};

LumpCatalog::LumpCatalog()
    : d(new Impl(this))
{}
    
LumpCatalog::LumpCatalog(const LumpCatalog &other)
    : d(new Impl(this, *other.d))
{}

void LumpCatalog::clear()
{
    d->clear();
}

bool LumpCatalog::setPackages(const StringList &packageIds)
{
    if (packageIds != d->packageIds)
    {
        d->packageIds = packageIds;
        d->updateBundles();
        return true;
    }
    return false;
}

void LumpCatalog::setBundles(const QList<const DataBundle *> &bundles)
{
    d->packageIds.clear();
    d->bundles = bundles;
}

LumpCatalog::LumpPos LumpCatalog::find(const String &lumpName) const
{
    return d->findLump(lumpName);
}

StringList LumpCatalog::packages() const
{
   return d->packageIds;
}

Block LumpCatalog::read(const String &lumpName) const
{
    return read(d->findLump(lumpName));
}
    
Block LumpCatalog::read(const LumpPos &lump) const
{
    Block data;
    if (lump.first)
    {
        DENG2_ASSERT(lump.first->lumpDirectory());
        auto const &entry = lump.first->lumpDirectory()->entry(lump.second);
        data.copyFrom(*lump.first, entry.offset, entry.size);
    }
    return data;
}

String LumpCatalog::lumpName(const LumpPos &lump) const
{
    if (lump.first)
    {
        DENG2_ASSERT(lump.first->lumpDirectory());
        return String::fromLatin1(lump.first->lumpDirectory()->entry(lump.second).name);
    }
    return {};
}

} // namespace res
