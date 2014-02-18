/** @file saveinfo.h  Saved game session info.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_SAVEINFO
#define LIBCOMMON_SAVEINFO
#ifdef __cplusplus

#include "doomsday.h"
#include "common.h"
#include <de/Path>
#include <de/String>

/**
 * Represents a saved game session state.
 *
 * @ingroup libcommon
 */
class SaveInfo
{
public:
#if !__JHEXEN__
    // Info data about players present (or not) in the game session.
    typedef byte Players[MAXPLAYERS];
#endif

public:
    SaveInfo();
    SaveInfo(SaveInfo const &other);

    static SaveInfo *newWithCurrentSessionMetadata(de::String userDescription);

    SaveInfo &operator = (SaveInfo const &other);

    /**
     * Determines whether the saved game session is compatibile with the current game session
     * (and @em should therefore be loadable).
     */
    bool isLoadable();

    /**
     * Attempt to update the save info from a saved game session. If the given file @a path
     * is invalid or the saved game state could not be recognized the save info is returned
     * to a valid but non-loadable state.
     *
     * @param path  Path to the resource file containing the game session header.
     *
     * @see isLoadable()
     */
    void updateFromFile(de::Path path);

    /**
     * Update the metadata associated with the save using values derived from the current game
     * session. Note that this does @em not affect the copy of this save on disk.
     */
    void applyCurrentSessionMetadata();

    /**
     * Returns the unique "identity key" of the game session.
     */
    de::String const &gameIdentityKey() const;
    void setGameIdentityKey(de::String newGameIdentityKey);

    /**
     * Returns the logical version of the serialized game session state.
     */
    int version() const;
    void setVersion(int newVersion);

    /**
     * Returns the textual description of the game session provided by the user.
     */
    de::String const &userDescription() const;
    void setUserDescription(de::String newDescription);

    /**
     * @see G_GenerateSessionId()
     */
    uint sessionId() const;
    void setSessionId(uint newSessionId);

    /**
     * Returns the URI of the @em current map of the game session.
     */
    Uri const *mapUri() const;
    void setMapUri(Uri const *newMapUri);

#if !__JHEXEN__

    /**
     * Returns the expired time in tics since the @em current map of the game session began.
     */
    int mapTime() const;
    void setMapTime(int newMapTime);

    /**
     * Returns the player info data for the game session.
     */
    Players const &players() const;
    void setPlayers(Players const &newPlayers);

#endif // !__JHEXEN__

    /**
     * Returns the game ruleset for the game session.
     */
    GameRuleset const &gameRules() const;
    void setGameRules(GameRuleset const &newRules);

    /**
     * Serializes the game session header using @a writer.
     */
    void write(Writer *writer) const;

    /**
     * Deserializes the game session header using @a reader.
     */
    void read(Reader *reader);

public: /// @todo refactor away:
    int magic() const;
    void setMagic(int newMagic);
    static SaveInfo *fromReader(Reader *reader);

private:
    DENG2_PRIVATE(d)
};

#endif // __cplusplus
#endif // LIBCOMMON_SAVEINFO
