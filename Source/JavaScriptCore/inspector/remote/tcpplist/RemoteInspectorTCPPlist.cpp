#include "config.h"
#include "RemoteInspector.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteAutomationTarget.h"
#include "RemoteConnectionToTarget.h"
#include "RemoteInspectionTarget.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <plist/Dictionary.h>
#include <plist/String.h>
#include <plist/Boolean.h>
#include <plist/Integer.h>
#include <plist/Data.h>
#include <runtime/InitializeThreading.h>
#include <WTF/wtf/MainThread.h>
#include <future>
#include <iostream>
#include "RemoteInspectorConstants.h"

namespace Inspector {

    RemoteInspector &RemoteInspector::singleton() {
        static NeverDestroyed<RemoteInspector> shared;
        return shared;
    }

    RemoteInspector::RemoteInspector() {
        start();
    }

    void RemoteInspector::updateAutomaticInspectionCandidate(RemoteInspectionTarget *target) {
        ASSERT_ARG(target, target);

        std::lock_guard<Lock> lock(m_mutex);

        unsigned targetIdentifier = target->targetIdentifier();
        if (!targetIdentifier)
            return;

        auto result = m_targetMap.set(targetIdentifier, target);
        ASSERT_UNUSED(result, !result.isNewEntry);

        // If the target has just allowed remote control, then the listing won't exist yet.
        // If the target has no identifier remove the old listing.
        if (std::shared_ptr<PList::Dictionary> targetListing = listingForTarget(*target))
            m_targetListingMap.set(targetIdentifier, targetListing);
        else
            m_targetListingMap.remove(targetIdentifier);

        pushListingsSoon();
    }

    void RemoteInspector::sendAutomaticInspectionCandidateMessage() {
        // not implemented
    }

    void RemoteInspector::sendMessageToRemote(unsigned targetIdentifier, const String &message) {
        if (!m_tcpServer)
            return;

        auto targetConnection = m_targetConnectionMap.get(targetIdentifier);
        if (!targetConnection)
            return;

        PList::Dictionary userInfo = PList::Dictionary();

        userInfo.Set("WIRRawDataKey", PList::String(message.utf8().data()));
        userInfo.Set("WIRConnectionIdentifierKey", PList::String(targetConnection->connectionIdentifier()));
        userInfo.Set("WIRDestinationKey", PList::String(targetConnection->destination()));

        m_tcpServer->sendMessage("WIRRawDataMessage", userInfo);
    }

    void RemoteInspector::start() {
        std::lock_guard<Lock> lock(m_mutex);

        if (m_enabled)
            return;

        m_enabled = true;

        setupTCPServerIfNeeded();
    }

    void RemoteInspector::stopInternal(StopSource _source) {
        std::lock_guard<Lock> lock(m_mutex);

        if (!m_enabled)
            return;

        m_enabled = false;
        m_pushScheduled = false;

        for (const auto &targetConnection : m_targetConnectionMap.values())
            targetConnection->close();
        m_targetConnectionMap.clear();

        updateHasActiveDebugSession();

        if (m_tcpServer) {
            m_tcpServer->close();
            m_tcpServer = nullptr;
        }
    }

    void RemoteInspector::setupTCPServerIfNeeded() {
        if (m_tcpServer)
            return;

        m_tcpServer = std::shared_ptr<RemoteInspectorTCPServer>(new RemoteInspectorTCPServer(this));
        pushListingsSoon();
    }


    wi_status RemoteInspector::receivedPList(wi_t self, const plist_t rpc_dict) {
        PList::Dictionary dict = PList::Dictionary(rpc_dict);
        ((RemoteInspectorTCPServer::Client *) self->context)->processReceivedPList(&dict);
        return WI_SUCCESS;
    }

    void RemoteInspector::processReceivedPList(PList::Dictionary *dict) {
        // TODO error support
        PList::String *messageName = (PList::String *) (*dict)["messageName"];
        PList::Dictionary *userInfo = (PList::Dictionary *) (*dict)["msgData"];
        std::lock_guard<Lock> lock(m_mutex);

        if (messageName->GetValue() == "WIRSocketDataMessage")
            receivedDataMessage(userInfo);
        else if (messageName->GetValue() == "WIRSocketSetupMessage")
            receivedSetupMessage(userInfo);
        else if (messageName->GetValue() == "WIRWebPageCloseMessage")
            receivedDidCloseMessage(userInfo);
        else if (messageName->GetValue() == "WIRApplicationGetListingMessage")
            receivedGetListingMessage(userInfo);
//        plist_free(dict->GetPlist());
    }

    TargetListing RemoteInspector::listingForInspectionTarget(const RemoteInspectionTarget &target) const {
        if (!target.remoteDebuggingAllowed())
            return nullptr;

        std::shared_ptr<PList::Dictionary> listing = std::shared_ptr<PList::Dictionary>(new PList::Dictionary());
        listing->Set("WIRTargetIdentifierKey", PList::Integer(target.targetIdentifier()));

        switch (target.type()) {
            case RemoteInspectionTarget::Type::JavaScript:
                listing->Set("WIRTitleKey", PList::String(target.name().utf8().data()));
                listing->Set("WIRTypeKey", PList::String("WIRTypeJavaScript"));
                break;
            case RemoteInspectionTarget::Type::Web:
                // not implemented
                ASSERT_NOT_REACHED();
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
        }

        if (auto *connectionToTarget = m_targetConnectionMap.get(target.targetIdentifier()))
            listing->Set("WIRConnectionIdentifierKey", PList::String(connectionToTarget->connectionIdentifier()));

        return listing;
    }

    TargetListing RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget &target) const {
        // not implemented
        return nullptr;
    }

    void RemoteInspector::pushListingsNow() {
        if (!m_tcpServer)
            return;
        m_pushScheduled = false;

        PList::Dictionary listings = PList::Dictionary();

        for (const std::shared_ptr<PList::Dictionary> listing : m_targetListingMap.values()) {
            PList::Integer *targetIdentifierKey = (PList::Integer *) (*listing)["WIRTargetIdentifierKey"];
            listings.Set(std::to_string(targetIdentifierKey->GetValue()), (*listing));
        }

        PList::Dictionary message = PList::Dictionary();
        message.Set("WIRListingKey", listings);

        m_tcpServer->sendMessage("WIRListingMessage", message);
    }

    void RemoteInspector::pushListingsSoon() {
        if (!m_tcpServer)
            return;

        if (m_pushScheduled)
            return;

        m_pushScheduled = true;

        std::thread([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::lock_guard<Lock> lock(m_mutex);
            if (m_pushScheduled)
                pushListingsNow();
        }).detach();
    }

    void RemoteInspector::receivedSetupMessage(PList::Dictionary *userInfo) {
        if (!((*userInfo)["WIRTargetIdentifierKey"]))
            return;
        unsigned targetIdentifier = ((PList::Integer *) ((*userInfo)["WIRTargetIdentifierKey"]))->GetValue();

        if (!(*userInfo)["WIRConnectionIdentifierKey"])
            return;
        PList::String *connectionIdentifier = (PList::String *) (*userInfo)["WIRConnectionIdentifierKey"];

        if (!(*userInfo)["WIRSenderKey"])
            return;
        PList::String *sender = (PList::String *) (*userInfo)["WIRSenderKey"];

        if (m_targetConnectionMap.contains(targetIdentifier))
            return;

        auto findResult = m_targetMap.find(targetIdentifier);
        if (findResult == m_targetMap.end())
            return;

        // Attempt to create a connection. This may fail if the page already has an inspector or if it disallows inspection.
        RemoteControllableTarget &target = *(findResult->value);
        auto connectionToTarget = adoptRef(*new RemoteConnectionToTarget(target, *connectionIdentifier, *sender));

        if (is<RemoteInspectionTarget>(target)) {
            if (!connectionToTarget->setup(false, false)) {
                connectionToTarget->close();
                return;
            }
            m_targetConnectionMap.set(targetIdentifier, WTFMove(connectionToTarget));
        } else if (is<RemoteAutomationTarget>(target)) {
            // not implemented
        } else
            ASSERT_NOT_REACHED();

        updateHasActiveDebugSession();
    }

    void RemoteInspector::receivedDataMessage(PList::Dictionary *userInfo) {
        if (!(*userInfo)["WIRTargetIdentifierKey"])
            return;
        unsigned targetIdentifier = ((PList::Integer *) (*userInfo)["WIRTargetIdentifierKey"])->GetValue();

        auto connectionToTarget = m_targetConnectionMap.get(targetIdentifier);
        if (!connectionToTarget)
            return;

        PList::String *data = (PList::String *) (*userInfo)["WIRSocketDataKey"];
        connectionToTarget->sendMessageToTarget(String(data->GetValue().c_str()));
    }

    void RemoteInspector::receivedDidCloseMessage(PList::Dictionary *userInfo) {
        if (!(*userInfo)["WIRTargetIdentifierKey"])
            return;
        unsigned targetIdentifier = ((PList::Integer *) (*userInfo)["WIRTargetIdentifierKey"])->GetValue();

        if (!((*userInfo)["WIRConnectionIdentifierKey"]))
            return;
        PList::String *connectionIdentifier = (PList::String *) (*userInfo)["WIRConnectionIdentifierKey"];

        auto connectionToTarget = m_targetConnectionMap.get(targetIdentifier);
        if (!connectionToTarget)
            return;

        if (connectionIdentifier->GetValue() != connectionToTarget->connectionIdentifier())
            return;

        connectionToTarget->close();
        m_targetConnectionMap.remove(targetIdentifier);

        updateHasActiveDebugSession();
    }

    void RemoteInspector::clientConnectionDied() {
        std::lock_guard<Lock> lock(m_mutex);

        auto it = m_targetConnectionMap.begin();
        auto end = m_targetConnectionMap.end();
        for (; it != end; ++it) {
            auto connection = it->value;
            connection->close();
            m_targetConnectionMap.remove(it);
        }
    }

    void RemoteInspector::receivedGetListingMessage(PList::Dictionary *userInfo) {
        pushListingsNow();
    }
} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
