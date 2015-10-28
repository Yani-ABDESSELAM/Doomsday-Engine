/** @file dummydriver.cpp  Dummy audio driver.
 *
 * @authors Copyright © 2003-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "audio/drivers/dummydriver.h"

#include "audio/channel.h"
#include "world/thinkers.h"
#include <de/Log>
#include <de/Observers>
#include <de/timer.h>
#include <QList>
#include <QtAlgorithms>

using namespace de;

namespace audio {

DummyDriver::CdChannel::CdChannel() : audio::CdChannel()
{}

Channel &DummyDriver::CdChannel::setVolume(dfloat)
{
    return *this;
}

bool DummyDriver::CdChannel::isPaused() const
{
    return _paused;
}

void DummyDriver::CdChannel::pause()
{
    _paused = true;
}

void DummyDriver::CdChannel::resume()
{
    _paused = false;
}

void DummyDriver::CdChannel::stop()
{}

Channel::PlayingMode DummyDriver::CdChannel::mode() const
{
    return _mode;
}

void DummyDriver::CdChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(_track < 0) Error("DummyDriver::CdChannel::play", "No track is bound");
    _mode = mode;
}

void DummyDriver::CdChannel::bindTrack(dint track)
{
    _track = de::max(-1, track);
}

// --------------------------------------------------------------------------------------

DummyDriver::MusicChannel::MusicChannel() : audio::MusicChannel()
{}

Channel &DummyDriver::MusicChannel::setVolume(dfloat)
{
    return *this;
}

bool DummyDriver::MusicChannel::isPaused() const
{
    return _paused;
}

void DummyDriver::MusicChannel::pause()
{
    _paused = true;
}

void DummyDriver::MusicChannel::resume()
{
    _paused = false;
}

void DummyDriver::MusicChannel::stop()
{}

Channel::PlayingMode DummyDriver::MusicChannel::mode() const
{
    return _mode;
}

void DummyDriver::MusicChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(_sourcePath.isEmpty()) Error("DummyDriver::MusicChannel::play", "No track is bound");
    _mode = mode;
}

bool DummyDriver::MusicChannel::canPlayBuffer() const
{
    return false;  /// @todo Should support this...
}

void *DummyDriver::MusicChannel::songBuffer(duint)
{
    return nullptr;
}

bool DummyDriver::MusicChannel::canPlayFile() const
{
    return true;
}

void DummyDriver::MusicChannel::bindFile(String const &sourcePath)
{
    _sourcePath = sourcePath;
}

// --------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(DummyDriver::SoundChannel)
, DENG2_OBSERVES(System, FrameEnds)
{
    bool noUpdate    = false;  ///< @c true if skipping updates (when stopped, before deletion).

    PlayingMode playingMode = { NotPlaying };
    dint startTime   = 0;      ///< When playback last started (Ticks).
    duint endTime    = 0;      ///< When playback last ended if not looping (Milliseconds).

    Positioning positioning = { StereoPositioning };
    dfloat frequency = 1;      ///< {0..1} Frequency/pitch adjustment factor.
    dfloat volume    = 1;      ///< {0..1} Volume adjustment factor.

    struct EmitterData
    {
        bool noOrigin            = true;   ///< @c true if the originator is some mystical emitter.
        bool noVolumeAttenuation = true;   ///< @c true if (distance based) volume attenuation is disabled.

        SoundEmitter *tracking = nullptr;  ///< Emitter to track, if any (not owned).
        Vector3d origin;                   ///< Emit from here (synced with emitter).

        void updateOriginIfNeeded()
        {
            // Only if we are tracking an emitter.
            if(!tracking) return;

            origin = Vector3d(tracking->origin);

            // When tracking a map-object set the Z axis position to the object's center.
            if(Thinker_IsMobjFunc(tracking->thinker.function))
            {
                origin.z += ((mobj_t *)tracking)->height / 2;
            }
        }
    } emitter;

    struct Buffer
    {
        sfxsample_t *data   = nullptr;
        bool needReloadData = false;

        dint sampleBytes = 1;      ///< Bytes per sample (1 or 2).
        dint sampleRate  = 11025;  ///< Number of samples per second.

        duint milliseconds(dfloat frequency) const
        {
            if(!data) return 0;
            return 1000 * data->numSamples / sampleRate * frequency;
        }

        void unload()
        {
            data = nullptr;
            needReloadData = false;
        }

        void load(sfxsample_t *sample)
        {
            data = sample;
            needReloadData = false;
            // Now the buffer is ready for playing.
        }

        void reloadIfNeeded()
        {
            if(needReloadData)
            {
                DENG2_ASSERT(data);
                load(data);
            }
        }
    } buffer;

    bool bufferIsValid = true;  ///< @c true when @var buffer matches the last requested format.

    Instance()
    {
        // We want notification when the frame ends in order to flush deferred property writes.
        System::get().audienceForFrameEnds() += this;
    }

    ~Instance()
    {
        // Cancel frame notifications.
        System::get().audienceForFrameEnds() -= this;
    }

    /**
     * Flushes property changes to the data buffer.
     *
     * @param force  Usually updates are only necessary during playback. Use @c true to
     * override this check and write the properties regardless.
     */
    void updateBuffer(bool force = false)
    {
        // Disabled?
        if(noUpdate) return;

        // Updates are only necessary during playback.
        if(playingMode != NotPlaying || force)
        {
            // When tracking an emitter we need the latest origin coordinates.
            emitter.updateOriginIfNeeded();
        }
    }

    void systemFrameEnds(System &)
    {
        updateBuffer();
    }
};

DummyDriver::SoundChannel::SoundChannel()
    : audio::SoundChannel()
    , d(new Instance)
{}

Channel::PlayingMode DummyDriver::SoundChannel::mode() const
{
    return d->playingMode;
}

void DummyDriver::SoundChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;

    if(mode == NotPlaying) return;

    d->buffer.reloadIfNeeded();
    // Playing is quite impossible without a loaded sample.
    if(!d->buffer.data)
    {
        throw Error("DummyDriver::SoundChannel", "No sample is bound");
    }

    // Flush deferred property value changes to the assigned data buffer.
    d->updateBuffer(true/*force*/);

    // Playback begins!
    d->playingMode = mode;

    // Remember the current time.
    d->startTime = Timer_Ticks();

    // Predict when the first/only playback cycle will end (in milliseconds).
    d->endTime = Timer_RealMilliseconds() + d->buffer.milliseconds(d->frequency);
}

void DummyDriver::SoundChannel::stop()
{
    // Playback ends forthwith!
    d->playingMode = NotPlaying;
    d->buffer.needReloadData = true;  // If subsequently started again.
}

bool DummyDriver::SoundChannel::isPaused() const
{
    return false;  // Never...
}

void DummyDriver::SoundChannel::pause()
{
    // Never paused...
}

void DummyDriver::SoundChannel::resume()
{
    // Never paused...
}

SoundEmitter *DummyDriver::SoundChannel::emitter() const
{
    return d->emitter.tracking;
}

dfloat DummyDriver::SoundChannel::frequency() const
{
    return d->frequency;
}

Vector3d DummyDriver::SoundChannel::origin() const
{
    return d->emitter.origin;
}

Positioning DummyDriver::SoundChannel::positioning() const
{
    return d->positioning;
}

dfloat DummyDriver::SoundChannel::volume() const
{
    return d->volume;
}

audio::SoundChannel &DummyDriver::SoundChannel::setEmitter(SoundEmitter *newEmitter)
{
    d->emitter.tracking = newEmitter;
    return *this;
}

audio::SoundChannel &DummyDriver::SoundChannel::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

audio::SoundChannel &DummyDriver::SoundChannel::setOrigin(Vector3d const &newOrigin)
{
    d->emitter.origin = newOrigin;
    return *this;
}

Channel &DummyDriver::SoundChannel::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

dint DummyDriver::SoundChannel::flags() const
{
    dint flags = 0;

    if(d->emitter.noOrigin)            flags |= SFXCF_NO_ORIGIN;
    if(d->emitter.noVolumeAttenuation) flags |= SFXCF_NO_ATTENUATION;

    if(d->noUpdate) flags |= SFXCF_NO_UPDATE;

    return flags;
}

void DummyDriver::SoundChannel::setFlags(dint flags)
{
    d->emitter.noOrigin            = CPP_BOOL(flags & SFXCF_NO_ORIGIN);
    d->emitter.noVolumeAttenuation = CPP_BOOL(flags & SFXCF_NO_ATTENUATION);

    d->noUpdate = CPP_BOOL(flags & SFXCF_NO_UPDATE);
}

void DummyDriver::SoundChannel::update()
{
    // Playback of non-looping sounds must stop when the first playback cycle ends.
    if(isPlaying() && !isPlayingLooped() && Timer_RealMilliseconds() >= d->endTime)
    {
        stop();
    }
}

void DummyDriver::SoundChannel::reset()
{
    stop();
    d->buffer.unload();
}

bool DummyDriver::SoundChannel::format(Positioning positioning, dint bytesPer, dint rate)
{
    // Do we need to (re)configure the sample data buffer?
    if(   d->positioning        != positioning
       || d->buffer.sampleRate  != rate
       || d->buffer.sampleBytes != bytesPer)
    {
        stop();
        DENG2_ASSERT(!isPlaying());

        d->positioning = positioning;

        de::zap(d->buffer);
        d->buffer.sampleBytes = bytesPer;
        d->buffer.sampleRate  = rate;
        d->bufferIsValid = true;
    }
    return isValid();
}

bool DummyDriver::SoundChannel::isValid() const
{
    return d->bufferIsValid;
}

void DummyDriver::SoundChannel::load(sfxsample_t const &sample)
{
    // Don't reload if a sample with the same sound ID is already loaded.
    if(!d->buffer.data || d->buffer.data->soundId != sample.soundId)
    {
        d->buffer.load(&const_cast<sfxsample_t &>(sample));
    }
}

dint DummyDriver::SoundChannel::bytes() const
{
    return d->buffer.sampleBytes;
}

dint DummyDriver::SoundChannel::rate() const
{
    return d->buffer.sampleRate;
}

sfxsample_t const *DummyDriver::SoundChannel::samplePtr() const
{
    return d->buffer.data;
}

dint DummyDriver::SoundChannel::startTime() const
{
    return d->startTime;
}

duint DummyDriver::SoundChannel::endTime() const
{
    return d->endTime;
}

void DummyDriver::SoundChannel::updateEnvironment()
{
    // Not supported.
}

// --------------------------------------------------------------------------------------

DENG2_PIMPL(DummyDriver)
{
    bool initialized = false;

    struct ChannelSet : public QList<Channel *>
    {
        ~ChannelSet() { DENG2_ASSERT(isEmpty()); }
    } channels[PlaybackInterfaceTypeCount];

    struct CdPlayer : public IPlayer
    {
        bool initialized = false;

        de::dint initialize()
        {
            return initialized = true;
        }

        void deinitialize()
        {
            initialized = false;
        }
    } cd;

    struct MusicPlayer : public IPlayer
    {
        bool initialized = false;

        dint initialize()
        {
            return initialized = true;
        }

        void deinitialize()
        {
            initialized = false;
        }
    } music;

    struct SoundPlayer : public ISoundPlayer
    {
        bool initialized = false;

        ~SoundPlayer() { DENG2_ASSERT(!initialized); }

        bool anyRateAccepted() const
        {
            // We are not playing any audio so yeah, whatever.
            return initialized;
        }

        dint initialize()
        {
            return initialized = true;
        }

        void deinitialize()
        {
            if(!initialized) return;

            initialized = false;
        }

        void allowRefresh(bool allow)
        {
            // We are not playing any audio so consider it done.
        }
    } sound;

    Instance(Public *i) : Base(i) {}

    ~Instance() { DENG2_ASSERT(!initialized); }

    void clearChannels()
    {
        for(ChannelSet &set : channels)
        {
            qDeleteAll(set);
            set.clear();
        }
    }
};

DummyDriver::DummyDriver() : d(new Instance(this))
{}

DummyDriver::~DummyDriver()
{
    deinitialize();  // If necessary.
}

void DummyDriver::initialize()
{
    LOG_AS("DummyDriver");

    // Already been here?
    if(d->initialized) return;

    d->initialized = true;
}

void DummyDriver::deinitialize()
{
    LOG_AS("DummyDriver");

    // Already been here?
    if(!d->initialized) return;

    d->cd.deinitialize();
    d->music.deinitialize();
    d->sound.deinitialize();
    d->clearChannels();

    d->initialized = false;
}

audio::System::IDriver::Status DummyDriver::status() const
{
    if(d->initialized) return Initialized;
    return Loaded;
}

String DummyDriver::identityKey() const
{
    return "dummy";
}

String DummyDriver::title() const
{
    return "Dummy Driver";
}

QList<Record> DummyDriver::listInterfaces() const
{
    QList<Record> list;
    {
        Record rec;
        rec.addNumber("type",        AUDIO_ICD);
        rec.addText  ("identityKey", DotPath(identityKey()) / "cd");
        list << rec;  // A copy is made.
    }
    {
        Record rec;
        rec.addNumber("type",        AUDIO_IMUSIC);
        rec.addText  ("identityKey", DotPath(identityKey()) / "music");
        list << rec;
    }
    {
        Record rec;
        rec.addNumber("type",        AUDIO_ISFX);
        rec.addText  ("identityKey", DotPath(identityKey()) / "sfx");
        list << rec;
    }
    return list;
}

IPlayer &DummyDriver::findPlayer(String interfaceIdentityKey) const
{
    if(IPlayer *found = tryFindPlayer(interfaceIdentityKey)) return *found;
    /// @throw UnknownInterfaceError  Unknown interface referenced.
    throw UnknownInterfaceError("DummyDriver::findPlayer", "Unknown playback interface \"" + interfaceIdentityKey + "\"");
}

IPlayer *DummyDriver::tryFindPlayer(String interfaceIdentityKey) const
{
    interfaceIdentityKey = interfaceIdentityKey.toLower();

    if(interfaceIdentityKey == "cd")    return &d->cd;
    if(interfaceIdentityKey == "music") return &d->music;
    if(interfaceIdentityKey == "sfx")   return &d->sound;

    return nullptr;  // Not found.
}

Channel *DummyDriver::makeChannel(PlaybackInterfaceType type)
{
    if(!d->initialized)
        return nullptr;

    switch(type)
    {
    case AUDIO_ICD:
        // Initialize this interface now if we haven't already.
        if(d->cd.initialize())
        {
            std::unique_ptr<Channel> channel(new CdChannel);
            d->channels[type] << channel.get();
            return channel.release();
        }
        break;

    case AUDIO_IMUSIC:
        // Initialize this interface now if we haven't already.
        if(d->music.initialize())
        {
            std::unique_ptr<Channel> channel(new MusicChannel);
            d->channels[type] << channel.get();
            return channel.release();
        }
        break;

    case AUDIO_ISFX:
        // Initialize this interface now if we haven't already.
        if(d->sound.initialize())
        {
            std::unique_ptr<Channel> channel(new SoundChannel);
            d->channels[type] << channel.get();
            return channel.release();
        }
        break;

    default: break;
    }

    return nullptr;
}

LoopResult DummyDriver::forAllChannels(PlaybackInterfaceType type,
    std::function<LoopResult (Channel const &)> callback) const
{
    for(Channel *ch : d->channels[type])
    {
        if(auto result = callback(*ch))
            return result;
    }
    return LoopContinue;
}

}  // namespace audio
