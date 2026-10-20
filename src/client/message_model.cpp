#include "message_model.hpp"

#include <QDateTime>

MessageModel::MessageModel(QObject *parent) : QAbstractListModel(parent) {}

int MessageModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(messages_.size());
}

QVariant MessageModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto &message = messages_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case NameRole:
        return QString::fromStdString(message.displayName);
    case TextRole:
        return QString::fromStdString(message.text);
    case TimeRole:
        return QDateTime::fromMSecsSinceEpoch(message.timestampMs)
            .toString(QStringLiteral("HH:mm"));
    default:
        return {};
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const {
    return {{NameRole, "displayName"}, {TextRole, "body"}, {TimeRole, "timeLabel"}};
}

void MessageModel::clear() {
    beginResetModel();
    messages_.clear();
    endResetModel();
}

void MessageModel::apply(const qchat::rpc::MessagePage &page) {
    if (page.reset) {
        clear();
    }
    for (const auto &message : page.messages) {
        if (!messages_.empty() && message.id <= messages_.back().id) {
            continue;
        }
        const auto row = rowCount();
        beginInsertRows({}, row, row);
        messages_.push_back(message);
        endInsertRows();
    }
    constexpr std::size_t capacity = 1000;
    if (messages_.size() > capacity) {
        const auto remove = messages_.size() - capacity;
        beginRemoveRows({}, 0, static_cast<int>(remove) - 1);
        messages_.erase(messages_.begin(), messages_.begin() + static_cast<std::ptrdiff_t>(remove));
        endRemoveRows();
    }
}
