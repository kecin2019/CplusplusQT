#include "ImageView.h"
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
ImageView::ImageView(QWidget *parent) : QGraphicsView(parent), m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setDragMode(QGraphicsView::NoDrag);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    setBackgroundBrush(QColor(244, 246, 250));
}

// void ImageView::setImage(const QImage &img)
// {
//     const bool had = (m_pix != nullptr);
//     const QTransform keep = transform();

//     if (!m_pix)
//     {
//         m_pix = m_scene->addPixmap(QPixmap::fromImage(img));
//     }
//     else
//     {
//         m_pix->setPixmap(QPixmap::fromImage(img));
//     }
//     m_scene->setSceneRect(m_pix->boundingRect());

//     if (had)
//     {
//         // 保持用户当前缩放/平移
//         setTransform(keep);
//     }
//     else
//     {
//         // 首次显示：1:1，不再 fitInView，避免被缩得很小
//         setTransform(QTransform());
//         setAlignment(Qt::AlignCenter);
//         centerOn(m_pix);
//     }
// }

void ImageView::setImage(const QImage &img, bool fitToWindow)
{
    if (!m_pix)
        m_pix = m_scene->addPixmap(QPixmap::fromImage(img));
    else
        m_pix->setPixmap(QPixmap::fromImage(img));
    m_scene->setSceneRect(m_pix->boundingRect());
    if (fitToWindow || !transform().isScaling())
        resetZoom(); // 首次或要求 fit：自适应视口
                     // 否则保持用户当前缩放/平移
}

void ImageView::clearImage()
{
    if (m_pix)
    {
        m_scene->removeItem(m_pix);
        delete m_pix;
        m_pix = nullptr;
    }
    m_scene->clear();
    m_scale = 1.0;
}

void ImageView::setSliceNavigationEnabled(bool enabled)
{
    m_sliceNavigationEnabled = enabled;
}
void ImageView::resetZoom()
{
    resetTransform();
    m_scale = 1.0;
    fitInView(m_scene->itemsBoundingRect(), Qt::KeepAspectRatio);
}
void ImageView::wheelEvent(QWheelEvent *e)
{
    if (m_sliceNavigationEnabled && !(e->modifiers() & Qt::ControlModifier))
    {
        int steps = (e->angleDelta().y() > 0) ? -1 : 1;
        emit sliceStepRequested(steps);
        e->accept();
        return;
    }

    const double factor = (e->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    m_scale *= factor;
    scale(factor, factor);
}
void ImageView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        m_panning = true;
        m_lastPos = e->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
    }
    QGraphicsView::mousePressEvent(e);
}
void ImageView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsView::mouseReleaseEvent(e);
}
void ImageView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_panning)
    {
        const QPointF p = e->position();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - (int(p.x()) - m_lastPos.x()));
        verticalScrollBar()->setValue(verticalScrollBar()->value() - (int(p.y()) - m_lastPos.y()));
        m_lastPos = p.toPoint();
    }
    QGraphicsView::mouseMoveEvent(e);
}
