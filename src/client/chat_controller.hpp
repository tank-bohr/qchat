#pragma once

#include "message_model.hpp"
#include "qchat/network.hpp"

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

Q_DECLARE_METATYPE(qchat::rpc::MessagePage)

class NetworkWorker final : public QObject {
    Q_OBJECT
  public:
    explicit NetworkWorker(QObject *parent = nullptr);
  public slots:
    void open(const QString &address, int port);
    void close();
    void send(const QString &name, const QString &text);
    void poll();
  signals:
    void opened();
    void received(const qchat::rpc::MessagePage &page);
    void failed(const QString &message);
    void rejected(const QString &message);
    void sent();

  private:
    QTimer *timer_;
    std::unique_ptr<qchat::Connection> connection_;
    std::string server_id_;
    std::int64_t cursor_{};
};

class ChatController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(MessageModel *messages READ messages CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
  public:
    explicit ChatController(QObject *parent = nullptr);
    ~ChatController() override;
    [[nodiscard]] MessageModel *messages() { return &messages_; }
    [[nodiscard]] bool connected() const { return connected_; }
    [[nodiscard]] bool busy() const { return busy_; }
    [[nodiscard]] bool sending() const { return sending_; }
    [[nodiscard]] const QString &error() const { return error_; }
    Q_INVOKABLE void connectToServer(const QString &address, int port, const QString &name);
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void sendMessage(const QString &text);
  signals:
    void stateChanged();
    void messageSent();
    void openRequested(const QString &address, int port);
    void closeRequested();
    void sendRequested(const QString &name, const QString &text);

  private:
    MessageModel messages_;
    QThread thread_;
    NetworkWorker *worker_;
    QString name_;
    QString error_;
    bool connected_{};
    bool busy_{};
    bool sending_{};
};
