#pragma once
#include <QQuickFramebufferObject>
#include "MpvPlayer.h"

class MpvVideo : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(MpvPlayer *player READ player WRITE setPlayer NOTIFY playerChanged)
public:
    explicit MpvVideo(QQuickItem *parent = nullptr) : QQuickFramebufferObject(parent) { setMirrorVertically(true); }
    MpvPlayer *player() const { return m_player; }
    void setPlayer(MpvPlayer *player) { m_player = player; emit playerChanged(); }
    Renderer *createRenderer() const override;
signals:
    void playerChanged();
private:
    MpvPlayer *m_player = nullptr;
};
