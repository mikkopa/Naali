#include "StableHeaders.h"
#include "ServerObserver.h"

#include "ModuleLoggingFunctions.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include "WorldStream.h"
#include "MumbleVoipModule.h"
#include "EventManager.h"

namespace MumbleVoip
{

    ServerObserver::ServerObserver(Foundation::Framework* framework) : 
        framework_event_category_(0),
        framework_(framework),
        server_info_request_manager_(new QNetworkAccessManager())
    {
        connect(server_info_request_manager_, SIGNAL(finished(QNetworkReply*)), this, SLOT(OnMumbleServerInfoHttpResponse(QNetworkReply*)));
    }

    ServerObserver::~ServerObserver()
    {
        SAFE_DELETE(server_info_request_manager_);
    }

    bool ServerObserver::HandleEvent(event_category_id_t category_id, event_id_t event_id, Foundation::EventDataInterface* data)
    {
        if (!framework_event_category_ && framework_)
           framework_event_category_ = framework_->GetEventManager()->QueryEventCategory("Framework");

        if (category_id == framework_event_category_)
        {
            switch (event_id)
            {
                case Foundation::WORLD_STREAM_READY:
                {
                    ProtocolUtilities::WorldStreamReadyEvent *event_data = dynamic_cast<ProtocolUtilities::WorldStreamReadyEvent *>(data);
                    if (event_data)
                    {
                        boost::shared_ptr<ProtocolUtilities::WorldStream> current_world_stream_ = event_data->WorldStream;
                        ProtocolUtilities::ClientParameters client_info = current_world_stream_->GetInfo();
                        QString agent_id = QString(client_info.agentID.ToString().c_str());
                        QString grid_url = QString(client_info.gridUrl.c_str());
                        RequestMumbleServerInfo(grid_url, agent_id);
                    }
                    break;
                }
                default:
                    break;
            }
        }
        return false;
    }

    void ServerObserver::RequestMumbleServerInfo(QString grid_url, QString agent_id)
    {
        QString path = "mumble_server_info";
        QUrl url(grid_url);
        if (url.scheme().length() == 0)
            url.setUrl(QString("http://%1").arg(grid_url));
        url.setPath(path);

        QNetworkRequest request(url);
        request.setRawHeader("avatar_uuid",agent_id.toAscii());
        server_info_request_manager_->get(request);
    }

    void ServerObserver::OnMumbleServerInfoHttpResponse(QNetworkReply* reply)
    {
        if (reply->error() != QNetworkReply::NoError)
        {
            QString message = QString("Mumble server info not available for server %1").arg(reply->url().toString());
            MumbleVoipModule::LogInfo(message.toStdString());
            reply->abort();
            reply->deleteLater();
            return;
        }

        ServerInfo info;
        info.server = reply->rawHeader("Mumble-Server");
        info.version = reply->rawHeader("Mumble-Version");
        info.channel = reply->rawHeader("Mumble-Channel");
        info.user_name = reply->rawHeader("Mumble-User");
        info.password = reply->rawHeader("Mumble-Password");
        info.avatar_id = reply->rawHeader("Mumble-Avatar-Id");
        info.context_id = reply->rawHeader("Mumble-Context-Id");

        QString message = QString("Mumble server info received for %1").arg(reply->url().toString());
        MumbleVoipModule::LogDebug(message.toStdString());

        emit MumbleServerInfoReceived(info);
    }

} // end of namespace: MumbleVoip
