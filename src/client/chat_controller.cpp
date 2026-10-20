#include "chat_controller.hpp"
#include "qchat/store.hpp"

#include <QHostAddress>

NetworkWorker::NetworkWorker(QObject *parent) : QObject(parent), timer_(new QTimer(this)) {
    timer_->setInterval(500);
    connect(timer_, &QTimer::timeout, this, &NetworkWorker::poll);
}

void NetworkWorker::open(const QString &address, int port) {
    close();
    try {
        connection_ = std::make_unique<qchat::Connection>(address.toStdString(), port);
        const auto page = connection_->poll({}, 0);
        server_id_ = page.serverId;
        cursor_ = page.nextCursor;
        emit opened();
        emit received(page);
        timer_->start();
    } catch (const std::exception &error) {
        close();
        emit failed(QString::fromUtf8(error.what()));
    }
}

void NetworkWorker::close() {
    timer_->stop();
    connection_.reset();
    server_id_.clear();
    cursor_ = 0;
}

void NetworkWorker::send(const QString &name, const QString &text) {
    if (!connection_) {
        emit failed(QStringLiteral("Disconnected; reconnect before sending."));
        return;
    }
    try {
        (void)connection_->post(name.toStdString(), text.toStdString());
        emit sent();
        poll();
    } catch (const qchat::rpc::InvalidRequest &error) {
        emit rejected(QString::fromStdString(error.reason));
    } catch (const std::exception &) {
        close();
        emit failed(QStringLiteral(
            "Connection lost. Delivery is uncertain; check the room before retrying."));
    }
}

void NetworkWorker::poll() {
    if (!connection_) {
        return;
    }
    try {
        const auto page = connection_->poll(server_id_, cursor_);
        server_id_ = page.serverId;
        cursor_ = page.nextCursor;
        emit received(page);
        // Drain history quickly, then return to the normal polling interval.
        timer_->start(page.messages.size() == 100 ? 1 : 500);
    } catch (const std::exception &error) {
        close();
        emit failed(QString::fromUtf8(error.what()));
    }
}

ChatController::ChatController(QObject *parent)
    : QObject(parent), messages_(this), worker_(new NetworkWorker) {
    qRegisterMetaType<qchat::rpc::MessagePage>();
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(this, &ChatController::openRequested, worker_, &NetworkWorker::open);
    connect(this, &ChatController::closeRequested, worker_, &NetworkWorker::close);
    connect(this, &ChatController::sendRequested, worker_, &NetworkWorker::send);
    connect(worker_, &NetworkWorker::opened, this, [this] {
        connected_ = true;
        busy_ = false;
        emit stateChanged();
    });
    connect(worker_, &NetworkWorker::received, this, [this](const auto &page) {
        if (connected_) {
            messages_.apply(page);
        }
    });
    connect(worker_, &NetworkWorker::failed, this, [this](const QString &message) {
        connected_ = false;
        busy_ = false;
        sending_ = false;
        error_ = message;
        emit stateChanged();
    });
    connect(worker_, &NetworkWorker::rejected, this, [this](const QString &message) {
        sending_ = false;
        error_ = message;
        emit stateChanged();
    });
    connect(worker_, &NetworkWorker::sent, this, [this] {
        sending_ = false;
        emit stateChanged();
        emit messageSent();
    });
    thread_.start();
}

ChatController::~ChatController() {
    QMetaObject::invokeMethod(worker_, &NetworkWorker::close, Qt::BlockingQueuedConnection);
    thread_.quit();
    thread_.wait();
}

void ChatController::connectToServer(const QString &address, int port, const QString &name) {
    if (busy_ || connected_) {
        return;
    }
    const auto label = name.trimmed();
    const auto bytes = label.toStdString();
    if (QHostAddress(address).isNull() || port < 1 || port > 65535 || bytes.empty() ||
        bytes.size() > qchat::max_name_bytes || !qchat::valid_text(bytes, false)) {
        error_ =
            QStringLiteral("Enter an IP address, port 1–65535, and a name of 1–64 UTF-8 bytes.");
        emit stateChanged();
        return;
    }
    name_ = label;
    error_.clear();
    messages_.clear();
    busy_ = true;
    emit stateChanged();
    emit openRequested(address, port);
}

void ChatController::disconnectFromServer() {
    if (busy_ || sending_) {
        return;
    }
    connected_ = false;
    error_.clear();
    emit closeRequested();
    emit stateChanged();
}

void ChatController::sendMessage(const QString &text) {
    if (!connected_ || sending_) {
        return;
    }
    const auto bytes = text.toStdString();
    if (text.trimmed().isEmpty() || bytes.size() > qchat::max_text_bytes ||
        !qchat::valid_text(bytes, true)) {
        error_ =
            QStringLiteral("Enter a message of 1–4096 UTF-8 bytes without control characters.");
        emit stateChanged();
        return;
    }
    sending_ = true;
    error_.clear();
    emit stateChanged();
    emit sendRequested(name_, text);
}
