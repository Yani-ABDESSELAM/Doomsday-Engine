/** @file serverlink.cpp  Network connection to a server.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_platform.h"
#include "network/serverlink.h"
#include "network/masterserver.h"
#include "network/net_main.h"
#include "network/net_buf.h"
#include "network/net_demo.h"
#include "network/net_event.h"
#include "network/protocol.h"
#include "client/cl_def.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"
#include "dd_def.h"
#include "dd_main.h"
#include "clientapp.h"

#include <de/Async>
#include <de/BlockPacket>
#include <de/ByteRefArray>
#include <de/ByteSubArray>
#include <de/Garbage>
#include <de/GuiApp>
#include <de/LinkFile>
#include <de/Message>
#include <de/MessageDialog>
#include <de/PackageLoader>
#include <de/RecordValue>
#include <de/RemoteFeedRelay>
#include <de/RemoteFile>
#include <de/Socket>
#include <de/data/json.h>
#include <de/memory.h>
#include <de/shell/ServerFinder>
#include <doomsday/DoomsdayApp>
#include <doomsday/Games>

#include <QElapsedTimer>
#include <QTimer>

using namespace de;

enum LinkState
{
    None,
    Discovering,
    Pinging,
    WaitingForPong,
    QueryingMapOutline,
    WaitingForInfoResponse,
    Joining,
    WaitingForJoinResponse,
    InGame
};

static int const NUM_PINGS = 5;

static String const PATH_REMOTE_SERVER  = "/remote/server";
static String const PATH_REMOTE_PACKS   = "/remote/packs";

DENG2_PIMPL(ServerLink)
, DENG2_OBSERVES(Asset, StateChange)
{
    std::unique_ptr<shell::ServerFinder> finder; ///< Finding local servers.
    LinkState state;
    bool fetching;
    typedef QMap<Address, shell::ServerInfo> Servers;
    Servers discovered;
    Servers fromMaster;
    QElapsedTimer pingTimer;
    QList<TimeDelta> pings;
    int pingCounter;
    std::unique_ptr<GameProfile> serverProfile; ///< Profile used when joining.
    std::function<void (GameProfile const *)> profileResultCallback;
    std::function<void (Address, GameProfile const *)> profileResultCallbackWithAddress;
    String fileRepository;
    AssetGroup downloads;
    std::function<void ()> postDownloadCallback;
    LoopCallback mainCall; // for deferred actions

    Impl(Public *i, Flags flags)
        : Base(i)
        , state(None)
        , fetching(false)
    {
        if (flags & DiscoverLocalServers)
        {
            finder.reset(new shell::ServerFinder);
        }
    }

    void notifyDiscoveryUpdate()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(DiscoveryUpdate, i) i->linkDiscoveryUpdate(self());
        emit self().serversDiscovered();
    }

    bool handleInfoResponse(Block const &reply)
    {
        DENG2_ASSERT(state == WaitingForInfoResponse);

        // Address of the server where the info was received.
        Address svAddress = self().address();

        // Local addresses are all represented as "localhost".
        if (svAddress.isLocal()) svAddress.setHost(QHostAddress::LocalHost);

        // Close the connection; that was all the information we need.
        self().disconnect();

        // Did we receive what we expected to receive?
        if (reply.size() >= 5 && reply.startsWith("Info\n"))
        {
            try
            {
                QVariant const response = parseJSON(String::fromUtf8(reply.mid(5)));
                std::unique_ptr<Value> rec(Value::constructFrom(response.toMap()));
                if (!is<RecordValue>(*rec))
                {
                    throw Error("ServerLink::handleInfoResponse", "Failed to parse response contents");
                }
                shell::ServerInfo svInfo(*rec->as<RecordValue>().record());

                LOG_NET_VERBOSE("Discovered server at ") << svAddress;

                // Update with the correct address.
                svAddress = Address(svAddress.host(), svInfo.port());
                svInfo.setAddress(svAddress);
                svInfo.printToLog(0, true);

                discovered.insert(svAddress, svInfo);

                // Show the information in the console.
                LOG_NET_NOTE("%i server%s been found")
                        << discovered.size()
                        << (discovered.size() != 1 ? "s have" : " has");

                notifyDiscoveryUpdate();

                // If the server's profile is being acquired, do the callback now.
                if (profileResultCallback ||
                    profileResultCallbackWithAddress)
                {
                    if (prepareServerProfile(svAddress))
                    {
                        LOG_NET_MSG("Received server's game profile from ") << svAddress;
                        mainCall.enqueue([this, svAddress] ()
                        {
                            if (profileResultCallback)
                            {
                                profileResultCallback(serverProfile.get());
                                profileResultCallback = nullptr;
                            }
                            if (profileResultCallbackWithAddress)
                            {
                                profileResultCallbackWithAddress(svAddress, serverProfile.get());
                                profileResultCallbackWithAddress = nullptr;
                            }
                        });
                    }
                }
                return true;
            }
            catch (Error const &er)
            {
                LOG_NET_WARNING("Info reply from %s was invalid: %s") << svAddress << er.asText();
            }
        }
        else if (reply.size() >= 11 && reply.startsWith("MapOutline\n"))
        {
            try
            {
                shell::MapOutlinePacket outline;
                {
                    Block const data = Block(ByteSubArray(reply, 11)).decompressed();
                    Reader src(data);
                    src.withHeader() >> outline;
                }
                DENG2_FOR_PUBLIC_AUDIENCE2(MapOutline, i)
                {
                    i->mapOutlineReceived(svAddress, outline);
                }
            }
            catch (Error const &er)
            {
                LOG_NET_WARNING("MapOutline reply from %s was invalid: %s") << svAddress << er.asText();
            }
        }
        else
        {
            LOG_NET_WARNING("Reply from %s was invalid") << svAddress;
        }
        return false;
    }

    /**
     * Handles the server's response to a client's join request.
     * @return @c false to stop processing further messages.
     */
    bool handleJoinResponse(Block const &reply)
    {
        if (reply.size() < 5 || reply != "Enter")
        {
            LOG_NET_WARNING("Server refused connection");
            LOGDEV_NET_WARNING("Received %i bytes instead of \"Enter\")")
                    << reply.size();
            self().disconnect();
            return false;
        }

        // We'll switch to joined mode.
        // Clients are allowed to send packets to the server.
        state = InGame;

        handshakeReceived = false;
        allowSending = true;
        netGame = true;             // Allow sending/receiving of packets.
        isServer = false;
        isClient = true;

        // Call game's NetConnect.
        gx.NetConnect(false);

        DENG2_FOR_PUBLIC_AUDIENCE2(Join, i) i->networkGameJoined();

        // G'day mate!  The client is responsible for beginning the handshake.
        Cl_SendHello();

        return true;
    }

    void fetchFromMaster()
    {
        if (fetching) return;

        fetching = true;
        N_MAPost(MAC_REQUEST);
        N_MAPost(MAC_WAIT);
        mainCall.enqueue([this] () { checkMasterReply(); });
    }

    void checkMasterReply()
    {
        DENG2_ASSERT(fetching);

        if (N_MADone())
        {
            fetching = false;

            fromMaster.clear();
            int const count = N_MasterGet(0, 0);
            for (int i = 0; i < count; i++)
            {
                shell::ServerInfo info;
                N_MasterGet(i, &info);
                fromMaster.insert(info.address(), info);
            }

            notifyDiscoveryUpdate();
        }
        else
        {
            // Check again later.
            mainCall.enqueue([this] () { checkMasterReply(); });
        }
    }

    bool prepareServerProfile(Address const &host)
    {
        serverProfile.reset();

        // Do we have information about this host?
        shell::ServerInfo info;
        if (!self().foundServerInfo(host, info))
        {
            return false;
        }
        if (info.gameId().isEmpty() || info.packages().isEmpty())
        {
            // There isn't enough information here.
            return false;
        }

        serverProfile.reset(new GameProfile(info.name()));
        serverProfile->setGame(info.gameId());
        serverProfile->setUseGameRequirements(false); // server lists all packages (that affect gameplay)
        serverProfile->setPackages(info.packages());

        return true; // profile available
    }

    Servers allFound(FoundMask const &mask) const
    {
        Servers all;
        if (finder && mask.testFlag(LocalNetwork))
        {
            // Append the ones from the server finder.
            foreach (Address const &sv, finder->foundServers())
            {
                all.insert(sv, finder->messageFromServer(sv));
            }
        }
        if (mask.testFlag(MasterServer))
        {
            // Append from master (if available).
            for (auto i = fromMaster.constBegin(); i != fromMaster.constEnd(); ++i)
            {
                all.insert(i.key(), i.value());
            }
        }
        if (mask.testFlag(Direct))
        {
            for (auto i = discovered.constBegin(); i != discovered.constEnd(); ++i)
            {
                all.insert(i.key(), i.value());
            }
        }
        return all;
    }

    void reportError(String const &msg)
    {
        // Show the error message in a dialog box.
        MessageDialog *dlg = new MessageDialog;
        dlg->setDeleteAfterDismissed(true);
        dlg->title().setText(tr("Cannot Join Game"));
        dlg->message().setText(msg);
        dlg->buttons() << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept);
        ClientWindow::main().root().addOnTop(dlg);
        dlg->open(MessageDialog::Modal);
    }

    void mountFileRepository(shell::ServerInfo const &info)
    {
        fileRepository = info.address().asText();
        FS::get().makeFolderWithFeed
                (PATH_REMOTE_SERVER,
                 RemoteFeedRelay::get().addServerRepository(fileRepository),
                 Folder::PopulateAsyncFullTree);
    }

    void unmountFileRepository()
    {
        unlinkRemotePackages();
        if (Folder *remoteFiles = FS::tryLocate<Folder>(PATH_REMOTE_SERVER))
        {
            trash(remoteFiles);
        }
        RemoteFeedRelay::get().removeRepository(fileRepository);
        fileRepository.clear();
    }

    void downloadFile(File &file)
    {
        if (auto *folder = maybeAs<Folder>(file))
        {
            folder->forContents([this] (String, File &f)
            {
                downloadFile(f);
                return LoopContinue;
            });
        }
        if (auto *remoteFile = maybeAs<RemoteFile>(file))
        {
            qDebug() << "downloading" << remoteFile->name();
            downloads.insert(*remoteFile);
            remoteFile->fetchContents();
        }
    }

    void assetStateChanged(Asset &) override
    {
        DENG2_ASSERT(!downloads.isEmpty());
        if (downloads.isReady())
        {
            qDebug() << downloads.size() << "downloads complete!";
            Loop::mainCall([this] ()
            {
                if (postDownloadCallback) postDownloadCallback();
            });
        }
    }

    void linkRemotePackages(StringList const &ids)
    {
        auto &fs = FS::get();

        qDebug() << fs.locate<Folder const>(PATH_REMOTE_SERVER).contentsAsText().toUtf8().constData();

        qDebug() << "registering remote packages...";

        Folder &remotePacks = FS::get().makeFolder(PATH_REMOTE_PACKS);
        foreach (String pkgId, ids)
        {
            qDebug() << "registering remote:" << pkgId;
            if (auto const *file = fs.tryLocate<File const>(PATH_REMOTE_SERVER / pkgId))
            {
                auto *pack = LinkFile::newLinkToFile(*file, file->name() + ".pack");
                pack->objectNamespace().add("package",
                        new Record(file->objectNamespace().subrecord("package")));
                remotePacks.add(pack);
                fs.index(*pack);
                qDebug() << "=>" << pack->path();
            }
        }
    }

    void unlinkRemotePackages()
    {
        // Unload all server packages. Note that the entire folder will be destroyed, too.
        if (std::unique_ptr<Folder> remotePacks { FS::tryLocate<Folder>(PATH_REMOTE_PACKS) })
        {
            remotePacks->forContents([] (String, File &file)
            {
                qDebug() << "unloading remote package:" << file.name();
                PackageLoader::get().unload(Package::identifierForFile(file));
                return LoopContinue;
            });
        }
    }

    DENG2_PIMPL_AUDIENCE(DiscoveryUpdate)
    DENG2_PIMPL_AUDIENCE(PingResponse)
    DENG2_PIMPL_AUDIENCE(MapOutline)
    DENG2_PIMPL_AUDIENCE(Join)
    DENG2_PIMPL_AUDIENCE(Leave)
};

DENG2_AUDIENCE_METHOD(ServerLink, DiscoveryUpdate)
DENG2_AUDIENCE_METHOD(ServerLink, PingResponse)
DENG2_AUDIENCE_METHOD(ServerLink, MapOutline)
DENG2_AUDIENCE_METHOD(ServerLink, Join)
DENG2_AUDIENCE_METHOD(ServerLink, Leave)

ServerLink::ServerLink(Flags flags) : d(new Impl(this, flags))
{
    if (d->finder)
    {
        connect(d->finder.get(), SIGNAL(updated()), this, SLOT(localServersFound()));
    }
    connect(this, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
    connect(this, SIGNAL(disconnected()), this, SLOT(linkDisconnected()));
}

void ServerLink::clear()
{
    d->finder->clear();
    // TODO: clear all found servers
}

void ServerLink::connectToServerAndChangeGameAsync(shell::ServerInfo info)
{
    // Automatically leave the current MP game.
    if (netGame && isClient)
    {
        disconnect();
    }

    // Forming the connection is a multi-step asynchronous procedure. The app's event
    // loop is running between each of these steps so the UI is never frozen. The first
    // step is to acquire the server's game profile.

    acquireServerProfileAsync(info.address(),
                              [this, info] (GameProfile const *serverProfile)
    {
        if (!serverProfile)
        {
            // Hmm, oopsie?
            String const errorMsg = QString("Not enough information known about server %1")
                    .arg(info.address().asText());
            LOG_NET_ERROR("Failed to join: ") << errorMsg;
            d->reportError(errorMsg);
            return;
        }

        // Profile acquired! Figure out if the profile can be started locally.

        auto &win = ClientWindow::main();
        win.glActivate();

        // If additional packages are configured, set up the ad-hoc profile with the
        // local additions.
        GameProfile const *joinProfile = serverProfile;
        auto const localPackages = serverProfile->game().localMultiplayerPackages();
        if (localPackages.count())
        {
            // Make sure the packages are available.
            for (String const &pkg : localPackages)
            {
                if (!PackageLoader::get().select(pkg))
                {
                    String const errorMsg = tr("The configured local multiplayer "
                                               "package %1 is unavailable.")
                            .arg(Package::splitToHumanReadable(pkg));
                    LOG_NET_ERROR("Failed to join %s: ") << info.address() << errorMsg;
                    d->reportError(errorMsg);
                    return;
                }
            }

            auto &adhoc = DoomsdayApp::app().adhocProfile();
            adhoc = *serverProfile;
            adhoc.setPackages(serverProfile->packages() + localPackages);
            joinProfile = &adhoc;
        }

        // The server makes certain packages available for clients to download.
        d->mountFileRepository(info);

        // Wait async until remote files have been populated so we can decide if
        // anything needs to be downloaded.
        async([] ()
        {
            Folder::waitForPopulation(); // remote feeds populating...
            return 0;
        },
        [this, joinProfile, info] (int)
        {
            // Now we know all the files that the server will be providing.
            // If we are missing any of the packages, download a copy from the server.

            qDebug() << "Folder population is complete";
            qDebug() << joinProfile->packages();
            qDebug() << "isPlayable:" << joinProfile->isPlayable();

            // Request contents of missing packages.
            d->downloads.clear();
            foreach (String missing, joinProfile->unavailablePackages())
            {
                if (File *rem = FS::tryLocate<File>(PATH_REMOTE_SERVER / missing))
                {
                    d->downloadFile(*rem);
                }
            }

            // We must wait after the downloads have finished.
            // The user sees a popup while downloads are progressing, and has the
            // option of cancelling.

            d->postDownloadCallback = [this, joinProfile, info] ()
            {
                auto &win = ClientWindow::main();
                win.glActivate();

                // Let's finalize the downloads so we can load all the packages.
                d->downloads.audienceForStateChange() -= d;
                d->downloads.clear();

                d->linkRemotePackages(joinProfile->unavailablePackages());

                if (!joinProfile->isPlayable())
                {
                    String const errorMsg = tr("Server's game \"%1\" is not playable on this system. "
                                               "The following packages are unavailable:\n\n%2")
                            .arg(info.gameId())
                            .arg(String::join(de::map(joinProfile->unavailablePackages(),
                                                      Package::splitToHumanReadable), "\n"));
                    LOG_NET_ERROR("Failed to join %s: ") << info.address() << errorMsg;
                    d->unmountFileRepository();
                    d->reportError(errorMsg);
                    return;
                }

                // Everything is finally good to go.

                BusyMode_FreezeGameForBusyMode();
                win.taskBar().close();
                DoomsdayApp::app().changeGame(*joinProfile, DD_ActivateGameWorker);
                connectHost(info.address());

                win.glDone();
            };

            // If nothing needs to be downloaded, let's just continue right away.
            if (d->downloads.isReady())
            {
                d->postDownloadCallback();
            }
            else
            {
                d->downloads.audienceForStateChange() += d;
            }
        });
    });
}

void ServerLink::acquireServerProfileAsync(Address const &address,
                                           std::function<void (GameProfile const *)> resultHandler)
{
    if (d->prepareServerProfile(address))
    {
        // We already know everything that is needed for the profile.
        d->mainCall.enqueue([this, resultHandler] ()
        {
            DENG2_ASSERT(d->serverProfile.get() != nullptr);
            resultHandler(d->serverProfile.get());
        });
    }
    else
    {
        AbstractLink::connectHost(address);
        d->profileResultCallback = resultHandler;
        d->state = Discovering;
        LOG_NET_MSG("Querying %s for full status") << address;
    }
}

void ServerLink::acquireServerProfileAsync(String const &domain,
                                           std::function<void (Address, GameProfile const *)> resultHandler)
{
    d->profileResultCallbackWithAddress = resultHandler;
    discover(domain);
    LOG_NET_MSG("Querying %s for full status") << domain;
}

void ServerLink::requestMapOutline(Address const &address)
{
    AbstractLink::connectHost(address);
    d->state = QueryingMapOutline;
    LOG_NET_VERBOSE("Querying %s for map outline") << address;
}

void ServerLink::ping(Address const &address)
{
    if (d->state != Pinging &&
        d->state != WaitingForPong)
    {
        AbstractLink::connectHost(address);
        d->state = Pinging;
    }
}

void ServerLink::connectDomain(String const &domain, TimeDelta const &timeout)
{
    LOG_AS("ServerLink::connectDomain");

    AbstractLink::connectDomain(domain, timeout);
    d->state = Joining;
}

void ServerLink::connectHost(Address const &address)
{
    LOG_AS("ServerLink::connectHost");

    AbstractLink::connectHost(address);
    d->state = Joining;
}

void ServerLink::linkDisconnected()
{
    LOG_AS("ServerLink");
    if (d->state != None)
    {
        LOG_NET_NOTE("Connection to server was disconnected");

        // Update our state and notify the game.
        disconnect();
    }
}

void ServerLink::disconnect()
{
    if (d->state == None) return;

    LOG_AS("ServerLink::disconnect");

    if (d->state == InGame)
    {
        d->state = None;

        // Tell the Game that a disconnection is about to happen.
        if (gx.NetDisconnect)
            gx.NetDisconnect(true);

        DENG2_FOR_AUDIENCE2(Leave, i) i->networkGameLeft();

        LOG_NET_NOTE("Link to server %s disconnected") << address();
        d->unmountFileRepository();
        AbstractLink::disconnect();

        Net_StopGame();

        // Tell the Game that the disconnection is now complete.
        if (gx.NetDisconnect)
            gx.NetDisconnect(false);
    }
    else
    {
        d->state = None;

        LOG_NET_NOTE("Connection attempts aborted");
        AbstractLink::disconnect();
    }
}

void ServerLink::discover(String const &domain)
{
    AbstractLink::connectDomain(domain, 5 /*timeout*/);

    d->discovered.clear();
    d->state = Discovering;
}

void ServerLink::discoverUsingMaster()
{
    d->fetchFromMaster();
}

bool ServerLink::isDiscovering() const
{
    return (d->state == Discovering ||
            d->state == WaitingForInfoResponse ||
            d->fetching);
}

int ServerLink::foundServerCount(FoundMask mask) const
{
    return d->allFound(mask).size();
}

QList<Address> ServerLink::foundServers(FoundMask mask) const
{
    return d->allFound(mask).keys();
}

bool ServerLink::isFound(Address const &host, FoundMask mask) const
{
    Address const addr = shell::checkPort(host);
    return d->allFound(mask).contains(addr);
}

bool ServerLink::foundServerInfo(int index, shell::ServerInfo &info, FoundMask mask) const
{
    Impl::Servers const all = d->allFound(mask);
    QList<Address> const listed = all.keys();
    if (index < 0 || index >= listed.size()) return false;
    info = all[listed[index]];
    return true;
}

bool ServerLink::isServerOnLocalNetwork(Address const &host) const
{
    if (!d->finder) return host.isLocal(); // Best guess...
    Address const addr = shell::checkPort(host);
    return d->finder->foundServers().contains(addr);
}

bool ServerLink::foundServerInfo(de::Address const &host, shell::ServerInfo &info, FoundMask mask) const
{
    Address const addr = shell::checkPort(host);
    Impl::Servers const all = d->allFound(mask);
    if (!all.contains(addr)) return false;
    info = all[addr];
    return true;
}

Packet *ServerLink::interpret(Message const &msg)
{
    return new BlockPacket(msg);
}

void ServerLink::initiateCommunications()
{
    switch (d->state)
    {
    case Discovering:
        // Ask for the serverinfo.
        *this << ByteRefArray("Info?", 5);
        d->state = WaitingForInfoResponse;
        break;

    case Pinging:
        *this << ByteRefArray("Ping?", 5);
        d->state = WaitingForPong;
        d->pingCounter = NUM_PINGS;
        d->pings.clear();
        d->pingTimer.start();
        break;

    case QueryingMapOutline:
        *this << ByteRefArray("MapOutline?", 11);
        d->state = WaitingForInfoResponse;
        break;

    case Joining: {
        ClientWindow::main().glActivate();

        Demo_StopPlayback();
        BusyMode_FreezeGameForBusyMode();

        // Tell the Game that a connection is about to happen.
        // The counterpart (false) call will occur after joining is successfully done.
        gx.NetConnect(true);

        // Connect by issuing: "Join (myname)"
        String pName = (playerName? playerName : "");
        if (pName.isEmpty())
        {
            pName = "Player";
        }
        String req = String("Join %1 %2").arg(SV_VERSION, 4, 16, QChar('0')).arg(pName);
        *this << req.toUtf8();

        d->state = WaitingForJoinResponse;

        ClientWindow::main().glDone();
        break; }

    default:
        DENG2_ASSERT(false);
        break;
    }
}

void ServerLink::localServersFound()
{
    d->notifyDiscoveryUpdate();
}

void ServerLink::handleIncomingPackets()
{
    if (d->state == Discovering)
        return;

    LOG_AS("ServerLink");
    forever
    {
        // Only BlockPackets received (see interpret()).
        QScopedPointer<BlockPacket> packet(static_cast<BlockPacket *>(nextPacket()));
        if (packet.isNull()) break;

        Block const &packetData = packet->block();

        switch (d->state)
        {
        case WaitingForInfoResponse:
            if (!d->handleInfoResponse(packetData)) return;
            break;

        case WaitingForJoinResponse:
            if (!d->handleJoinResponse(packetData)) return;
            break;

        case WaitingForPong:
            if (packetData.size() == 4 && packetData == "Pong" &&
                d->pingCounter-- > 0)
            {
                d->pings.append(TimeDelta::fromMilliSeconds(d->pingTimer.elapsed()));
                *this << ByteRefArray("Ping?", 5);
                d->pingTimer.restart();
            }
            else
            {
                Address const svAddress = address();

                disconnect();

                // Notify about the average ping time.
                if (d->pings.count())
                {
                    TimeDelta average = 0;
                    for (TimeDelta i : d->pings) average += i;
                    average /= d->pings.count();

                    DENG2_FOR_AUDIENCE2(PingResponse, i)
                    {
                        i->pingResponse(svAddress, average);
                    }
                }
            }
            break;

        case InGame: {
            /// @todo The incoming packets should be handled immediately.

            // Post the data into the queue.
            netmessage_t *msg = reinterpret_cast<netmessage_t *>(M_Calloc(sizeof(netmessage_t)));

            msg->sender = 0; // the server
            msg->data = new byte[packetData.size()];
            memcpy(msg->data, packetData.data(), packetData.size());
            msg->size = packetData.size();
            msg->handle = msg->data; // needs delete[]

            // The message queue will handle the message from now on.
            N_PostMessage(msg);
            break; }

        default:
            // Ignore any packets left.
            break;
        }
    }
}

ServerLink &ServerLink::get() // static
{
    return ClientApp::serverLink();
}
