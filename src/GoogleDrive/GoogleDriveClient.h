#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QRegularExpression>

// ─────────────────────────────────────────────────────────────────────────────
// GoogleDriveClient
//
// Handles OAuth2 (device flow) + Google Drive file upload.
// Tokens are persisted in QSettings so the user only logs in once.
//
// Usage:
//   1. Call ensureAuthenticated() — opens browser if no token stored
//   2. Call uploadFile(localPath) after a successful save
// ─────────────────────────────────────────────────────────────────────────────
class GoogleDriveClient : public QObject
{
    Q_OBJECT

public:
    explicit GoogleDriveClient(QObject *parent = nullptr);

    // Paste your credentials from Google Cloud Console here:
    static constexpr const char *kClientId     = "1082466359737-j60b1bnkp1tls3na7ipls76o3nbj238k.apps.googleusercontent.com";
    static constexpr const char *kClientSecret = "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";

    bool isAuthenticated() const;
    void ensureAuthenticated();   // triggers device-flow login if needed
    void logout();

    // Upload a local file to the PixelCanvas/ folder on Drive.
    // Emits uploadFinished(true/false, fileName).
    void uploadFile(const QString &localPath);

signals:
    void authenticated();                             // login completed
    void authFailed(const QString &reason);           // login failed
    void uploadFinished(bool ok, const QString &fileName);
    void uploadProgress(qint64 sent, qint64 total);

private slots:
    void onDeviceCodeReply();
    void onTokenReply();
    void onRefreshReply();
    void onFolderReply();
    void onUploadReply();
    void pollForToken();

private:
    void requestDeviceCode();
    void pollToken();
    void refreshAccessToken();
    void ensureDriveFolder();                         // find or create PixelCanvas/
    void doUpload(const QString &folderId);

    QNetworkAccessManager *m_nam          = nullptr;
    QTimer                *m_pollTimer    = nullptr;

    // OAuth state
    QString  m_deviceCode;
    QString  m_accessToken;
    QString  m_refreshToken;
    int      m_pollInterval = 5;          // seconds

    // Upload state
    QString  m_pendingUploadPath;         // file waiting to be uploaded
    QString  m_driveFolderId;             // cached folder id

    QTcpServer *m_localServer = nullptr;
    void onLocalRedirect();
    void exchangeCodeForToken(const QString &code);
};