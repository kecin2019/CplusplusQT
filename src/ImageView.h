#pragma once
#include <QGraphicsView>
#include <QImage>
#include <QGraphicsPixmapItem>
class ImageView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageView(QWidget *parent = nullptr);
    void setImage(const QImage &img);
    void clearImage();

protected:
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;

private:
    void resetZoom();
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pix{nullptr};
    double m_scale{1.0};
    QPoint m_lastPos;
    bool m_panning{false};
};
