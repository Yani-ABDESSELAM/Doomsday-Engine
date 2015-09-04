/** @file plugindriver.cpp  Plugin based audio driver.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#include "audio/drivers/plugindriver.h"

#include "api_audiod.h"
#include <de/Library>
#include <de/Log>
#include <de/NativeFile>

using namespace de;

namespace audio {

DENG2_PIMPL_NOREF(PluginDriver)
, DENG2_OBSERVES(audio::System, FrameBegins)
, DENG2_OBSERVES(audio::System, FrameEnds)
, DENG2_OBSERVES(audio::System, MidiFontChange)
{
    bool initialized   = false;
    ::Library *library = nullptr;  ///< Library instance (owned).

    /// @todo Extract this into a (base) Plugin class. -ds
    struct IPlugin
    {
        int (*Init) (void);
        void (*Shutdown) (void);
        void (*Event) (int type);
        int (*Get) (int prop, void *ptr);
        int (*Set) (int prop, void const *ptr);
    } iBase;

    audiointerface_sfx_t   iSfx;
    audiointerface_music_t iMusic;
    audiointerface_cd_t    iCd;

    Instance()
    {
        de::zap(iBase);
        de::zap(iSfx);
        de::zap(iMusic);
        de::zap(iCd);
    }

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);

        // Unload the library.
        Library_Delete(library);
    }

    /**
     * Lookup the value of a named @em string property from the driver.
     */
    String getPropertyAsString(dint prop)
    {
        DENG2_ASSERT(iBase.Get);
        ddstring_t str; Str_InitStd(&str);
        if(iBase.Get(prop, &str))
        {
            auto string = String(Str_Text(&str));
            Str_Free(&str);
            return string;
        }
        /// @throw ReadPropertyError  Driver returned not successful.
        throw ReadPropertyError("audio::PluginDriver::Instance::getPropertyAsString", "Error reading property:" + String::number(prop));
    }

    void systemFrameBegins(audio::System &)
    {
        DENG2_ASSERT(initialized);
        if(iSfx.gen.Init)   iBase.Event(SFXEV_BEGIN);
        if(iCd.gen.Init)    iCd.gen.Update();
        if(iMusic.gen.Init) iMusic.gen.Update();
    }

    void systemFrameEnds(audio::System &)
    {
        DENG2_ASSERT(initialized);
        iBase.Event(SFXEV_END);
    }

    void systemMidiFontChanged(String const &newMidiFontPath)
    {
        DENG2_ASSERT(initialized);
        DENG2_ASSERT(iBase.Set);
        iBase.Set(AUDIOP_SOUNDFONT_FILENAME, newMidiFontPath.toLatin1().constData());
    }
};

PluginDriver::PluginDriver() : d(new Instance)
{}

PluginDriver::~PluginDriver()
{
    LOG_AS("~audio::PluginDriver");
    deinitialize();  // If necessary.
}

PluginDriver *PluginDriver::newFromLibrary(LibraryFile &libFile)  // static
{
    try
    {
        std::unique_ptr<PluginDriver> driver(new PluginDriver);
        PluginDriver &inst = *driver;

        inst.d->library = Library_New(libFile.path().toUtf8().constData());
        if(!inst.d->library)
        {
            /// @todo fixme: Should not be failing here! -ds
            return nullptr;
        }

        de::Library &lib = libFile.library();

        lib.setSymbolPtr( inst.d->iBase.Init,     "DS_Init");
        lib.setSymbolPtr( inst.d->iBase.Shutdown, "DS_Shutdown");
        lib.setSymbolPtr( inst.d->iBase.Event,    "DS_Event");
        lib.setSymbolPtr( inst.d->iBase.Get,      "DS_Get");
        lib.setSymbolPtr( inst.d->iBase.Set,      "DS_Set", de::Library::OptionalSymbol);

        if(lib.hasSymbol("DS_SFX_Init"))
        {
            lib.setSymbolPtr( inst.d->iSfx.gen.Init,      "DS_SFX_Init");
            lib.setSymbolPtr( inst.d->iSfx.gen.Create,    "DS_SFX_CreateBuffer");
            lib.setSymbolPtr( inst.d->iSfx.gen.Destroy,   "DS_SFX_DestroyBuffer");
            lib.setSymbolPtr( inst.d->iSfx.gen.Load,      "DS_SFX_Load");
            lib.setSymbolPtr( inst.d->iSfx.gen.Reset,     "DS_SFX_Reset");
            lib.setSymbolPtr( inst.d->iSfx.gen.Play,      "DS_SFX_Play");
            lib.setSymbolPtr( inst.d->iSfx.gen.Stop,      "DS_SFX_Stop");
            lib.setSymbolPtr( inst.d->iSfx.gen.Refresh,   "DS_SFX_Refresh");
            lib.setSymbolPtr( inst.d->iSfx.gen.Set,       "DS_SFX_Set");
            lib.setSymbolPtr( inst.d->iSfx.gen.Setv,      "DS_SFX_Setv");
            lib.setSymbolPtr( inst.d->iSfx.gen.Listener,  "DS_SFX_Listener");
            lib.setSymbolPtr( inst.d->iSfx.gen.Listenerv, "DS_SFX_Listenerv");
            lib.setSymbolPtr( inst.d->iSfx.gen.Getv,      "DS_SFX_Getv", de::Library::OptionalSymbol);
        }

        if(lib.hasSymbol("DM_Music_Init"))
        {
            lib.setSymbolPtr( inst.d->iMusic.gen.Init,    "DM_Music_Init");
            lib.setSymbolPtr( inst.d->iMusic.gen.Update,  "DM_Music_Update");
            lib.setSymbolPtr( inst.d->iMusic.gen.Get,     "DM_Music_Get");
            lib.setSymbolPtr( inst.d->iMusic.gen.Set,     "DM_Music_Set");
            lib.setSymbolPtr( inst.d->iMusic.gen.Pause,   "DM_Music_Pause");
            lib.setSymbolPtr( inst.d->iMusic.gen.Stop,    "DM_Music_Stop");
            lib.setSymbolPtr( inst.d->iMusic.SongBuffer,  "DM_Music_SongBuffer", de::Library::OptionalSymbol);
            lib.setSymbolPtr( inst.d->iMusic.Play,        "DM_Music_Play",       de::Library::OptionalSymbol);
            lib.setSymbolPtr( inst.d->iMusic.PlayFile,    "DM_Music_PlayFile",   de::Library::OptionalSymbol);
        }

        if(lib.hasSymbol("DM_CDAudio_Init"))
        {
            lib.setSymbolPtr( inst.d->iCd.gen.Init,       "DM_CDAudio_Init");
            lib.setSymbolPtr( inst.d->iCd.gen.Update,     "DM_CDAudio_Update");
            lib.setSymbolPtr( inst.d->iCd.gen.Set,        "DM_CDAudio_Set");
            lib.setSymbolPtr( inst.d->iCd.gen.Get,        "DM_CDAudio_Get");
            lib.setSymbolPtr( inst.d->iCd.gen.Pause,      "DM_CDAudio_Pause");
            lib.setSymbolPtr( inst.d->iCd.gen.Stop,       "DM_CDAudio_Stop");
            lib.setSymbolPtr( inst.d->iCd.Play,           "DM_CDAudio_Play");
        }

        return driver.release();
    }
    catch(de::Library::SymbolMissingError const &er)
    {
        LOG_AS("audio::PluginDriver");
        LOG_AUDIO_ERROR("") << er.asText();
    }
    return nullptr;
}

bool PluginDriver::recognize(LibraryFile &library)  // static
{
    // By convention, driver plugin names use a standard prefix.
    if(!library.name().beginsWith("audio_")) return false;

    // Driver plugins are native files.
    if(!library.source()->is<NativeFile>()) return false;

    // This appears to be usuable with PluginDriver.
    /// @todo Open the library and ensure it's type matches.
    return true;
}

String PluginDriver::identityKey() const
{
    return d->getPropertyAsString(AUDIOP_IDENTITYKEY).toLower();
}

String PluginDriver::title() const
{
    return d->getPropertyAsString(AUDIOP_TITLE);
}

audio::System::IDriver::Status PluginDriver::status() const
{
    if(d->initialized) return Initialized;
    DENG2_ASSERT(d->iBase.Init != nullptr);
    return Loaded;
}

void PluginDriver::initialize()
{
    LOG_AS("audio::PluginDriver");

    // Already been here?
    if(d->initialized) return;

    DENG2_ASSERT(d->iBase.Init != nullptr);
    d->initialized = d->iBase.Init();

    // We want notification at various times:
    audioSystem().audienceForFrameBegins() += d;
    audioSystem().audienceForFrameEnds()   += d;
    if(d->iBase.Set)
    {
        audioSystem().audienceForMidiFontChange() += d;
    }
}

void PluginDriver::deinitialize()
{
    LOG_AS("audio::PluginDriver");

    // Already been here?
    if(!d->initialized) return;

    if(d->iBase.Shutdown)
    {
        d->iBase.Shutdown();
    }

    // Stop receiving notifications:
    if(d->iBase.Set)
    {
        audioSystem().audienceForMidiFontChange() -= d;
    }
    audioSystem().audienceForFrameEnds()   -= d;
    audioSystem().audienceForFrameBegins() -= d;

    d->initialized = false;
}

::Library *PluginDriver::library() const
{
    return d->library;
}

bool PluginDriver::hasCd() const
{
    return iCd().gen.Init != nullptr;
}

bool PluginDriver::hasMusic() const
{
    return iMusic().gen.Init != nullptr;
}

bool PluginDriver::hasSfx() const
{
    return iSfx().gen.Init != nullptr;
}

audiointerface_cd_t /*const*/ &PluginDriver::iCd() const
{
    return d->iCd;
}

audiointerface_music_t /*const*/ &PluginDriver::iMusic() const
{
    return d->iMusic;
}

audiointerface_sfx_t /*const*/ &PluginDriver::iSfx() const
{
    return d->iSfx;
}

DotPath PluginDriver::interfacePath(void *playbackInterface) const
{
    if((void *)&d->iSfx == playbackInterface)
    {
        /// @todo SFX interfaces aren't named yet.
        return identityKey() + ".unnamed_sfx";
    }

    if((void *)&d->iMusic == playbackInterface || (void *)&d->iCd == playbackInterface)
    {
        char buf[256];  /// @todo This could easily overflow...
        auto *gen = (audiointerface_music_generic_t *) playbackInterface;
        if(gen->Get(MUSIP_ID, buf))
        {
            return identityKey() + "." + String(buf);
        }
        else
        {
            DENG2_ASSERT(!"[MUSIP_ID not defined]");
            return identityKey() + ".unnamed_music";
        }
    }

    return "";  // Not recognized.
}

}  // namespace audio