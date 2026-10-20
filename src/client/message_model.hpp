#pragma once

#include "chat_types.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

class MessageModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
  public:
    enum Role : std::uint16_t { NameRole = Qt::UserRole + 1, TextRole, TimeRole };
    explicit MessageModel(QObject *parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void apply(const qchat::rpc::MessagePage &page);
    void clear();

  private:
    std::vector<qchat::rpc::Message> messages_;
};
