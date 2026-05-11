#include "GoogleDriveClient.h"

#include <QSslSocket>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QTimer>
#include <QSettings>
#include <QDesktopServices>
#include <QHttpMultiPart>

static constexpr const char *kTokenUrl   = "https://oauth2.googleapis.com/token";
static constexpr const char *kDeviceUrl  = "https://oauth2.googleapis.com/device/code";
static constexpr const char *kDriveFiles = "https://www.googleapis.com/drive/v3/files";
static constexpr const char *kUploadUrl  =
    "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart";
static constexpr const char *kScope      =
    "https://www.googleapis.com/auth/drive.file";
static constexpr const char *kFolderName = "PixelCanvas";
static constexpr const char *kRedirectUri = "http://localhost:8080";

// ─────────────────────────────────────────────────────────────────────────────
GoogleDriveClient::GoogleDriveClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &GoogleDriveClient::pollForToken);

    // Restore saved tokens
    QSettings s("YourOrg", "YourApp");
    m_accessToken   = s.value("gdrive/access_token").toString();
    m_refreshToken  = s.value("gdrive/refresh_token").toString();
    m_driveFolderId = s.value("gdrive/folder_id").toString();

    // ← add this: silently refresh on startup if we have a refresh token
    if (!m_refreshToken.isEmpty()) {
        refreshAccessToken();
    }

}
// ─────────────────────────────────────────────────────────────────────────────
bool GoogleDriveClient::isAuthenticated() const
{
    return !m_accessToken.isEmpty();
}

void GoogleDriveClient::logout()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_driveFolderId.clear();
    QSettings s("YourOrg", "YourApp");
    s.remove("gdrive/access_token");
    s.remove("gdrive/refresh_token");
    s.remove("gdrive/folder_id");
}

// ─────────────────────────────────────────────────────────────────────────────
// Auth — Device Authorization Flow
// (works on desktop Linux without a redirect URI / local HTTP server)
// ─────────────────────────────────────────────────────────────────────────────
void GoogleDriveClient::ensureAuthenticated()
{
    if (isAuthenticated()) {
        // Try refreshing silently first in case the access token expired
        if (!m_refreshToken.isEmpty())
            refreshAccessToken();
        else
            emit authenticated();
        return;
    }
    requestDeviceCode();
}

void GoogleDriveClient::requestDeviceCode()
{
    static constexpr const char *kAuthUrl    = "https://accounts.google.com/o/oauth2/v2/auth";
    static constexpr const char *kTokenUrl   = "https://oauth2.googleapis.com/token";
    static constexpr const char *kRedirectUri = "http://localhost:8080";

    // Start local server to catch the redirect
    m_localServer = new QTcpServer(this);
    m_localServer->listen(QHostAddress::LocalHost, 8080);
    connect(m_localServer, &QTcpServer::newConnection,
            this, &GoogleDriveClient::onLocalRedirect);

    QUrlQuery q;
    q.addQueryItem("client_id",     kClientId);
    q.addQueryItem("redirect_uri",  kRedirectUri);
    q.addQueryItem("response_type", "code");
    q.addQueryItem("scope",         kScope);
    q.addQueryItem("access_type",   "offline");

    QUrl url(kAuthUrl);
    url.setQuery(q);
    QDesktopServices::openUrl(url);
}

void GoogleDriveClient::onDeviceCodeReply()
    {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
     
    qDebug() << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "Response body:" << reply->readAll(); 

    if (reply->error() != QNetworkReply::NoError) {
        emit authFailed(reply->errorString());
        return;
    }

    const QJsonObject obj =
        QJsonDocument::fromJson(reply->readAll()).object();

    m_deviceCode   = obj["device_code"].toString();
    m_pollInterval = obj["interval"].toInt(5);

    const QString userCode  = obj["user_code"].toString();
    const QString verifyUrl = obj["verification_url"].toString();

    // Open browser and show the user code in a message box
    QDesktopServices::openUrl(QUrl(verifyUrl));

    // We just emit authFailed with instructions so MainWindow can show a dialog
    // (re-use the signal with a special prefix so MainWindow can detect it)
    emit authFailed(QString("DEVICE_CODE:%1:%2").arg(userCode, verifyUrl));

    // Start polling
    m_pollTimer->start(m_pollInterval * 1000);
}


void GoogleDriveClient::pollForToken()
{
    pollToken();
}

void GoogleDriveClient::pollToken()
{
    QNetworkRequest req{QUrl{kTokenUrl}};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("client_id",     kClientId);
    body.addQueryItem("client_secret", kClientSecret);
    body.addQueryItem("device_code",   m_deviceCode);
    body.addQueryItem("grant_type",    "urn:ietf:params:oauth2:grant_type:device_code");

    QNetworkReply *reply = m_nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, &GoogleDriveClient::onTokenReply);
}

void GoogleDriveClient::onTokenReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();

    const QJsonObject obj =
        QJsonDocument::fromJson(reply->readAll()).object();

    if (obj.contains("error")) {
        // "authorization_pending" is normal — keep polling
        if (obj["error"].toString() != "authorization_pending") {
            m_pollTimer->stop();
            emit authFailed(obj["error_description"].toString());
        }
        return;
    }

    m_pollTimer->stop();
    m_accessToken  = obj["access_token"].toString();
    m_refreshToken = obj["refresh_token"].toString();

    QSettings s("YourOrg", "YourApp");
    s.setValue("gdrive/access_token",  m_accessToken);
    s.setValue("gdrive/refresh_token", m_refreshToken);

    emit authenticated();
}

void GoogleDriveClient::refreshAccessToken()
{
    QNetworkRequest req{QUrl{kTokenUrl}};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("client_id",     kClientId);
    body.addQueryItem("client_secret", kClientSecret);
    body.addQueryItem("refresh_token", m_refreshToken);
    body.addQueryItem("grant_type",    "refresh_token");

    QNetworkReply *reply = m_nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, &GoogleDriveClient::onRefreshReply);
}

void GoogleDriveClient::onRefreshReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();

    const QJsonObject obj =
        QJsonDocument::fromJson(reply->readAll()).object();

    if (obj.contains("error")) {
        // Refresh token expired — need full re-auth via browser
        logout();
        requestDeviceCode();   // this now uses localhost:8080 flow
        return;
    }

    m_accessToken = obj["access_token"].toString();
    QSettings s("YourOrg", "YourApp");
    s.setValue("gdrive/access_token", m_accessToken);

    emit authenticated();
}

// ─────────────────────────────────────────────────────────────────────────────
// Upload
// ─────────────────────────────────────────────────────────────────────────────
void GoogleDriveClient::uploadFile(const QString &localPath)
{
    m_pendingUploadPath = localPath;

    if (!isAuthenticated()) {
        // Auth first, then upload in the authenticated() signal
        ensureAuthenticated();
        return;
    }

    if (m_driveFolderId.isEmpty())
        ensureDriveFolder();
    else
        doUpload(m_driveFolderId);
}

void GoogleDriveClient::ensureDriveFolder()
{
    // Search for existing PixelCanvas folder
    QUrl url(kDriveFiles);
    QUrlQuery q;
    q.addQueryItem("q",
        QString("name='%1' and mimeType='application/vnd.google-apps.folder' "
                "and trashed=false").arg(kFolderName));
    q.addQueryItem("fields", "files(id,name)");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + m_accessToken.toUtf8());

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, &GoogleDriveClient::onFolderReply);
}

void GoogleDriveClient::onFolderReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
       
    if (reply->error() != QNetworkReply::NoError) {
        // Access token may have expired — refresh and retry
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            refreshAccessToken();
            connect(this, &GoogleDriveClient::authenticated,
                    this, [this]{ ensureDriveFolder(); },
                    Qt::SingleShotConnection);
        } else {
            emit uploadFinished(false, QFileInfo(m_pendingUploadPath).fileName());
        }
        return;
    }

    const QJsonObject obj =
        QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray files = obj["files"].toArray();

    if (!files.isEmpty()) {
        // Folder already exists
        m_driveFolderId = files[0].toObject()["id"].toString();
        QSettings s("YourOrg", "YourApp");
        s.setValue("gdrive/folder_id", m_driveFolderId);
        doUpload(m_driveFolderId);
        return;
    }

    // Create the folder
    QNetworkRequest req{QUrl{kDriveFiles}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + m_accessToken.toUtf8());

    QJsonObject meta;
    meta["name"]     = kFolderName;
    meta["mimeType"] = "application/vnd.google-apps.folder";

    QNetworkReply *createReply =
        m_nam->post(req, QJsonDocument(meta).toJson());
    connect(createReply, &QNetworkReply::finished, this, [this, createReply]() {
        createReply->deleteLater();
        const QJsonObject obj =
            QJsonDocument::fromJson(createReply->readAll()).object();
        m_driveFolderId = obj["id"].toString();
        QSettings s("YourOrg", "YourApp");
        s.setValue("gdrive/folder_id", m_driveFolderId);
        doUpload(m_driveFolderId);
    });
}

void GoogleDriveClient::doUpload(const QString &folderId)
{
    QFile *file = new QFile(m_pendingUploadPath, this);
    if (!file->open(QIODevice::ReadOnly)) {
        file->deleteLater();
        emit uploadFinished(false, QFileInfo(m_pendingUploadPath).fileName());
        return;
    }

    const QString fileName = QFileInfo(m_pendingUploadPath).fileName();
    const QMimeDatabase mimeDb;
    const QString mimeType =
        mimeDb.mimeTypeForFile(m_pendingUploadPath).name();

    // Build multipart: JSON metadata part + file data part
    auto *multipart = new QHttpMultiPart(QHttpMultiPart::RelatedType, this);

    // -- Metadata part
    QJsonObject meta;
    meta["name"] = fileName;
    meta["parents"] = QJsonArray{ folderId };

    QHttpPart metaPart;
    metaPart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("application/json; charset=UTF-8"));
    metaPart.setBody(QJsonDocument(meta).toJson());
    multipart->append(metaPart);

    // -- File data part
    QHttpPart dataPart;
    dataPart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant(mimeType));
    dataPart.setBodyDevice(file);
    file->setParent(multipart);   // multipart owns file now
    multipart->append(dataPart);

    QNetworkRequest req{QUrl{kUploadUrl}};
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + m_accessToken.toUtf8());

    QNetworkReply *reply = m_nam->post(req, multipart);
    multipart->setParent(reply);  // reply owns multipart

    connect(reply, &QNetworkReply::uploadProgress,
            this,  &GoogleDriveClient::uploadProgress);
    connect(reply, &QNetworkReply::finished,
            this,  &GoogleDriveClient::onUploadReply);
}

void GoogleDriveClient::onUploadReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();

    const bool ok = (reply->error() == QNetworkReply::NoError);
    emit uploadFinished(ok, QFileInfo(m_pendingUploadPath).fileName());
    m_pendingUploadPath.clear();
}

void GoogleDriveClient::onLocalRedirect()
{
    QTcpSocket *socket = m_localServer->nextPendingConnection();
    socket->waitForReadyRead(3000);
    const QString request = QString::fromUtf8(socket->readAll());

    // Send success page to browser
    const QByteArray html =
        "<html><body><h2>PixelCanvas: Authenticated!</h2>"
        "<p>You can close this tab.</p></body></html>";
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + html);
    socket->flush();
    socket->deleteLater();
    m_localServer->close();

    // Extract code from "GET /?code=xxxx HTTP/1.1"
    const QRegularExpression re("code=([^& ]+)");
    const QRegularExpressionMatch match = re.match(request);
    if (!match.hasMatch()) {
        emit authFailed("No auth code in redirect.");
        return;
    }
    exchangeCodeForToken(match.captured(1));
}

void GoogleDriveClient::exchangeCodeForToken(const QString &code)
{
    QNetworkRequest req{QUrl{kTokenUrl}};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("client_id",     kClientId);
    body.addQueryItem("client_secret", kClientSecret);
    body.addQueryItem("code",          code);
    body.addQueryItem("redirect_uri",  kRedirectUri);
    body.addQueryItem("grant_type",    "authorization_code");

    QNetworkReply *reply = m_nam->post(
        req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished,
            this, &GoogleDriveClient::onTokenReply);
}