/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qscreentransformed_qws.h"

#ifndef QT_NO_QWS_TRANSFORMED
#include <qscreendriverfactory_qws.h>
#include <qvector.h>
#include <private/qpainter_p.h>
#include <private/qmemrotate_p.h>
#include <qmatrix.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include <qwindowsystem_qws.h>
#include <qwsdisplay_qws.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

//#define QT_REGION_DEBUG

#ifdef QT_REGION_DEBUG
#include <QDebug>
#endif

class QTransformedScreenPrivate
{
public:
    QTransformedScreenPrivate(QTransformedScreen *parent);

    void configure();

    QTransformedScreen::Transformation transformation;
#ifdef QT_QWS_DEPTH_GENERIC
    bool doGenericColors;
#endif
    QTransformedScreen *q;
};

QTransformedScreenPrivate::QTransformedScreenPrivate(QTransformedScreen *parent)
    : transformation(QTransformedScreen::None),
#ifdef QT_QWS_DEPTH_GENERIC
      doGenericColors(false),
#endif
      q(parent)
{
}

extern "C"
#ifndef QT_BUILD_GUI_LIB
Q_DECL_EXPORT
#endif
void qws_setScreenTransformation(QScreen *that, int t)
{
    QTransformedScreen *tscreen = static_cast<QTransformedScreen*>(that);
    tscreen->setTransformation((QTransformedScreen::Transformation)t);
}

// ---------------------------------------------------------------------------
// Transformed Screen
// ---------------------------------------------------------------------------

/*!
    \internal

    \class QTransformedScreen
    \ingroup qws

    \brief The QTransformedScreen class implements a screen driver for
    a transformed screen.

    Note that this class is only available in \l{Qt for Embedded Linux}.
    Custom screen drivers can be added by subclassing the
    QScreenDriverPlugin class, using the QScreenDriverFactory class to
    dynamically load the driver into the application, but there should
    only be one screen object per application.

    Use the QScreen::isTransformed() function to determine if a screen
    is transformed. The QTransformedScreen class itself provides means
    of rotating the screen with its setTransformation() function; the
    transformation() function returns the currently set rotation in
    terms of the \l Transformation enum (which describes the various
    available rotation settings). Alternatively, QTransformedScreen
    provides an implementation of the QScreen::transformOrientation()
    function, returning the current rotation as an integer value.

    \sa QScreen, QScreenDriverPlugin, {Running Applications}
*/

/*!
    \enum QTransformedScreen::Transformation

    This enum describes the various rotations a transformed screen can
    have.

    \value None No rotation
    \value Rot90 90 degrees rotation
    \value Rot180 180 degrees rotation
    \value Rot270 270 degrees rotation
*/

/*!
    \fn bool QTransformedScreen::isTransformed() const
    \reimp
*/

/*!
    Constructs a QTransformedScreen object. The \a displayId argument
    identifies the Qt for Embedded Linux server to connect to.
*/
QTransformedScreen::QTransformedScreen(int displayId)
    : QProxyScreen(displayId, QScreen::TransformedClass)
{
    d_ptr = new QTransformedScreenPrivate(this);
    d_ptr->transformation = None;

#ifdef QT_REGION_DEBUG
    qDebug() << "QTransformedScreen::QTransformedScreen";
#endif
}

void QTransformedScreenPrivate::configure()
{
    // ###: works because setTransformation recalculates unconditionally
    q->setTransformation(transformation);
}

/*!
    Destroys the QTransformedScreen object.
*/
QTransformedScreen::~QTransformedScreen()
{
    delete d_ptr;
}

static int getDisplayId(const QString &spec)
{
    QRegExp regexp(QLatin1String(":(\\d+)\\b"));
    if (regexp.lastIndexIn(spec) != -1) {
        const QString capture = regexp.cap(1);
        return capture.toInt();
    }
    return 0;
}

static QTransformedScreen::Transformation filterTransformation(QString &spec)
{
    QRegExp regexp(QLatin1String("\\bRot(\\d+):?\\b"), Qt::CaseInsensitive);
    if (regexp.indexIn(spec) == -1)
        return QTransformedScreen::None;

    const int degrees = regexp.cap(1).toInt();
    spec.remove(regexp.pos(0), regexp.matchedLength());

    return static_cast<QTransformedScreen::Transformation>(degrees / 90);
}

/*!
    \reimp
*/
bool QTransformedScreen::connect(const QString &displaySpec)
{
    QString dspec = displaySpec.trimmed();
    if (dspec.startsWith(QLatin1String("Transformed:"), Qt::CaseInsensitive))
        dspec = dspec.mid(QString::fromLatin1("Transformed:").size());
    else if (!dspec.compare(QLatin1String("Transformed"), Qt::CaseInsensitive))
        dspec = QString();

    const QString displayIdSpec = QString::fromLatin1(" :%1").arg(displayId);
    if (dspec.endsWith(displayIdSpec))
        dspec = dspec.left(dspec.size() - displayIdSpec.size());

    d_ptr->transformation = filterTransformation(dspec);

    QString driver = dspec;
    int colon = driver.indexOf(QLatin1Char(':'));
    if (colon >= 0)
        driver.truncate(colon);

    if (!QScreenDriverFactory::keys().contains(driver, Qt::CaseInsensitive))
        if (!dspec.isEmpty())
            dspec.prepend(QLatin1Char(':'));

    const int id = getDisplayId(dspec);
    QScreen *s = qt_get_screen(id, dspec.toLatin1().constData());
    setScreen(s);

#ifdef QT_QWS_DEPTH_GENERIC
    d_ptr->doGenericColors = dspec.contains(QLatin1String("genericcolors"));
#endif

    d_ptr->configure();

    // XXX
    qt_screen = this;

    return true;
}

/*!
    Returns the currently set rotation.

    \sa setTransformation(), QScreen::transformOrientation()
*/
QTransformedScreen::Transformation QTransformedScreen::transformation() const
{
    return d_ptr->transformation;
}

/*!
    \reimp
*/
int QTransformedScreen::transformOrientation() const
{
    return (int)d_ptr->transformation;
}

/*!
    \reimp
*/
void QTransformedScreen::exposeRegion(QRegion region, int changing)
{
    if (!data || d_ptr->transformation == None) {
        QProxyScreen::exposeRegion(region, changing);
        return;
    }
    QScreen::exposeRegion(region, changing);
}

/*!
    Rotates this screen object according to the specified \a transformation.

    \sa transformation()
*/
void QTransformedScreen::setTransformation(Transformation transformation)
{
    d_ptr->transformation = transformation;
    QSize size = mapFromDevice(QSize(dw, dh));
    w = size.width();
    h = size.height();

    const QScreen *s = screen();
    size = mapFromDevice(QSize(s->physicalWidth(), s->physicalHeight()));
    physWidth = size.width();
    physHeight = size.height();

#ifdef QT_REGION_DEBUG
    qDebug() << "QTransformedScreen::setTransformation" << transformation
             << "size" << w << h << "dev size" << dw << dh;
#endif

}

static inline QRect correctNormalized(const QRect &r) {
    const int x1 = qMin(r.left(), r.right());
    const int x2 = qMax(r.left(), r.right());
    const int y1 = qMin(r.top(), r.bottom());
    const int y2 = qMax(r.top(), r.bottom());

    return QRect( QPoint(x1,y1), QPoint(x2,y2) );
}

template <class DST, class SRC>
static inline void blit90(QScreen *screen, const QImage &image,
                          const QRect &rect, const QPoint &topLeft)
{
    const SRC *src = (const SRC*)(image.scanLine(rect.top())) + rect.left();
    DST *dest = (DST*)(screen->base() + topLeft.y() * screen->linestep())
                + topLeft.x();
    qt_memrotate90(src, rect.width(), rect.height(), image.bytesPerLine(),
                   dest, screen->linestep());
}

template <class DST, class SRC>
static inline void blit180(QScreen *screen, const QImage &image,
                           const QRect &rect, const QPoint &topLeft)
{
    const SRC *src = (const SRC*)(image.scanLine(rect.top())) + rect.left();
    DST *dest = (DST*)(screen->base() + topLeft.y() * screen->linestep())
                + topLeft.x();
    qt_memrotate180(src, rect.width(), rect.height(), image.bytesPerLine(),
                    dest, screen->linestep());
}

#include <linux/fb.h>
#include <sys/ioctl.h>

void setVarScreenInfo(int fh, struct fb_var_screeninfo *var)
{
    if(ioctl(fh, FBIOPUT_VSCREENINFO, var))
    {
        fprintf(stderr, "ioctl FBIOPUT_VSCREENINFO: %s\n", strerror(errno));
        exit(1);
    }
}

int g_LinuxFb = 0;
struct fb_var_screeninfo g_VarSI;
int g_CurrentScreen = 0;

template <class DST, class SRC>
static inline void blit270(QScreen *screen, const QImage &image,
                           const QRect &rect, const QPoint &topLeft)
{ 
#ifdef QT_DEBUG_DRAW
    qDebug("---blit270 begin---");
    qDebug("rect=(%d, %d), [%d x %d],topLeft=(%d, %d)",rect.x(),rect.y(),rect.width(),rect.height(),topLeft.x(),topLeft.y());
#endif
    const SRC *src = (const SRC *)(image.scanLine(rect.top())) + rect.left();
    DST *dest = (DST*)(screen->base() + topLeft.y() * screen->linestep())
                + topLeft.x();
//    DST *dest = (DST*)(screen->base() + rect.top() * screen->linestep())
//                + rect.left();
    if(g_LinuxFb != 0 && g_CurrentScreen == 1) {
        dest += 864 * 480;
    }
    qt_memrotate270(src, rect.width(), rect.height(), image.bytesPerLine(),
                    dest, screen->linestep());

    
#ifdef QT_DEBUG_DRAW
    qDebug("Image --- %p",&image);
    qDebug("---blit270 end---");
#endif
}

typedef void (*BlitFunc)(QScreen *, const QImage &, const QRect &, const QPoint &);

#define SET_BLIT_FUNC(dst, src, rotation, func) \
do {                                            \
    switch (rotation) {                         \
    case Rot90:                                 \
        func = blit90<dst, src>;                \
        break;                                  \
    case Rot180:                                \
        func = blit180<dst, src>;               \
        break;                                  \
    case Rot270:                                \
        func = blit270<dst, src>;               \
        break;                                  \
    default:                                    \
        break;                                  \
    }                                           \
} while (0)

QRegion g_LastRegion;

/*!
    \reimp
*/
void QTransformedScreen::blit(const QImage &image, const QPoint &topLeft,
                              const QRegion &region)
{
#ifdef QT_DEBUG_DRAW
    qDebug("---blit begin---");
    qDebug("Image --- %p",&image);
#endif
    const Transformation trans = d_ptr->transformation;
    if (trans == None) {
        QProxyScreen::blit(image, topLeft, region);
#ifdef QT_DEBUG_DRAW
	qDebug("---blit end---");
#endif
        return;
    }
    QRegion combineRegion = region | g_LastRegion;

    const QVector<QRect> rects = combineRegion.rects();
    const QRect bound = QRect(0, 0, QScreen::w, QScreen::h)
                        & QRect(topLeft, image.size());

    BlitFunc func = 0;
#ifdef QT_QWS_DEPTH_GENERIC
    if (d_ptr->doGenericColors && depth() == 16) {
        if (image.depth() == 16)
            SET_BLIT_FUNC(qrgb_generic16, quint16, trans, func);
        else
            SET_BLIT_FUNC(qrgb_generic16, quint32, trans, func);
    } else
#endif
    switch (depth()) {
#ifdef QT_QWS_DEPTH_32
    case 32:
#ifdef QT_QWS_DEPTH_16
        if (image.depth() == 16)
            SET_BLIT_FUNC(quint32, quint16, trans, func);
        else
#endif
            SET_BLIT_FUNC(quint32, quint32, trans, func);
        break;
#endif
#if defined(QT_QWS_DEPTH_24) || defined(QT_QWS_DEPTH18)
    case 24:
    case 18:
        SET_BLIT_FUNC(quint24, quint24, trans, func);
        break;
#endif
#if defined(QT_QWS_DEPTH_16) || defined(QT_QWS_DEPTH_15) || defined(QT_QWS_DEPTH_12)
    case 16:
#if defined QT_QWS_ROTATE_BGR
        if (pixelType() == BGRPixel && image.depth() == 16) {
            SET_BLIT_FUNC(qbgr565, quint16, trans, func);
            break;
        } //fall-through here!!!
#endif
    case 15:
#if defined QT_QWS_ROTATE_BGR
        if (pixelType() == BGRPixel && image.format() == QImage::Format_RGB555) {
            SET_BLIT_FUNC(qbgr555, qrgb555, trans, func);
            break;
        } //fall-through here!!!
#endif
    case 12:
        if (image.depth() == 16)
            SET_BLIT_FUNC(quint16, quint16, trans, func);
        else
            SET_BLIT_FUNC(quint16, quint32, trans, func);
        break;
#endif
#ifdef QT_QWS_DEPTH_8
    case 8:
        if (image.format() == QImage::Format_RGB444)
            SET_BLIT_FUNC(quint8, qrgb444, trans, func);
        else if (image.depth() == 16)
            SET_BLIT_FUNC(quint8, quint16, trans, func);
        else
            SET_BLIT_FUNC(quint8, quint32, trans, func);
        break;
#endif
    default:
#ifdef QT_DEBUG_DRAW
	qDebug("---blit end---");
#endif
        return;
    }
    if (!func)
    {
#ifdef QT_DEBUG_DRAW
	qDebug("---blit end---");
#endif
        return;
    }
    QWSDisplay::grab();

    g_LastRegion = region;
    for (int i = 0; i < rects.size(); ++i) {
        const QRect r = rects.at(i) & bound;
//        QRect r = QRect(0, 0, QScreen::w, QScreen::h);

        QPoint dst;
        switch (trans) {
        case Rot90:
            dst = mapToDevice(r.topRight(), QSize(w, h));
            break;
        case Rot180:
            dst = mapToDevice(r.bottomRight(), QSize(w, h));
            break;
        case Rot270:
            dst = mapToDevice(r.bottomLeft(), QSize(w, h));
            break;
        default:
            break;
        }
#ifdef QT_DEBUG_DRAW
    qDebug("blit---func(this, image, r, dst)");
#endif
//      func(this, image, r.translated(-topLeft), dst);
	func(this, image, r, dst);
        break;
    }
    if(g_LinuxFb != 0) {
        g_VarSI.yoffset = g_CurrentScreen * 864;
        setVarScreenInfo(g_LinuxFb, &g_VarSI);
        g_CurrentScreen = g_CurrentScreen == 0 ? 1 : 0;
    }
    QWSDisplay::ungrab();
#ifdef QT_DEBUG_DRAW
    qDebug("---blit end---");
#endif
}

/*!
    \reimp
*/
void QTransformedScreen::solidFill(const QColor &color, const QRegion &region)
{
    const QRegion tr = mapToDevice(region, QSize(w,h));

    Q_ASSERT(tr.boundingRect() == mapToDevice(region.boundingRect(), QSize(w,h)));

#ifdef QT_REGION_DEBUG
    qDebug() << "QTransformedScreen::solidFill region" << region << "transformed" << tr;
#endif
    QProxyScreen::solidFill(color, tr);
}

/*!
    \reimp
*/
QSize QTransformedScreen::mapToDevice(const QSize &s) const
{
    switch (d_ptr->transformation) {
    case None:
    case Rot180:
        break;
    case Rot90:
    case Rot270:
        return QSize(s.height(), s.width());
        break;
    }
    return s;
}

/*!
    \reimp
*/
QSize QTransformedScreen::mapFromDevice(const QSize &s) const
{
    switch (d_ptr->transformation) {
    case None:
    case Rot180:
        break;
    case Rot90:
    case Rot270:
        return QSize(s.height(), s.width());
        break;
    }
    return s;
}

/*!
    \reimp
*/
QPoint QTransformedScreen::mapToDevice(const QPoint &p, const QSize &s) const
{
    QPoint rp(p);

    switch (d_ptr->transformation) {
    case None:
        break;
    case Rot90:
        rp.setX(p.y());
        rp.setY(s.width() - p.x() - 1);
        break;
    case Rot180:
        rp.setX(s.width() - p.x() - 1);
        rp.setY(s.height() - p.y() - 1);
        break;
    case Rot270:
        rp.setX(s.height() - p.y() - 1);
        rp.setY(p.x());
        break;
    }

    return rp;
}

/*!
    \reimp
*/
QPoint QTransformedScreen::mapFromDevice(const QPoint &p, const QSize &s) const
{
    QPoint rp(p);

    switch (d_ptr->transformation) {
    case None:
        break;
    case Rot90:
        rp.setX(s.height() - p.y() - 1);
        rp.setY(p.x());
        break;
    case Rot180:
        rp.setX(s.width() - p.x() - 1);
        rp.setY(s.height() - p.y() - 1);
        break;
    case Rot270:
        rp.setX(p.y());
        rp.setY(s.width() - p.x() - 1);
        break;
    }

    return rp;
}

/*!
    \reimp
*/
QRect QTransformedScreen::mapToDevice(const QRect &r, const QSize &s) const
{
    if (r.isNull())
        return QRect();

    QRect tr;
    switch (d_ptr->transformation) {
    case None:
        tr = r;
        break;
    case Rot90:
        tr.setCoords(r.y(), s.width() - r.x() - 1,
                     r.bottom(), s.width() - r.right() - 1);
        break;
    case Rot180:
        tr.setCoords(s.width() - r.x() - 1, s.height() - r.y() - 1,
                     s.width() - r.right() - 1, s.height() - r.bottom() - 1);
        break;
    case Rot270:
        tr.setCoords(s.height() - r.y() - 1, r.x(),
                     s.height() - r.bottom() - 1, r.right());
        break;
    }

    return correctNormalized(tr);
}

/*!
    \reimp
*/
QRect QTransformedScreen::mapFromDevice(const QRect &r, const QSize &s) const
{
    if (r.isNull())
        return QRect();

    QRect tr;
    switch (d_ptr->transformation) {
    case None:
        tr = r;
        break;
    case Rot90:
        tr.setCoords(s.height() - r.y() - 1, r.x(),
                     s.height() - r.bottom() - 1, r.right());
        break;
    case Rot180:
        tr.setCoords(s.width() - r.x() - 1, s.height() - r.y() - 1,
                     s.width() - r.right() - 1, s.height() - r.bottom() - 1);
        break;
    case Rot270:
        tr.setCoords(r.y(), s.width() - r.x() - 1,
                     r.bottom(), s.width() - r.right() - 1);
        break;
    }

    return correctNormalized(tr);
}

/*!
    \reimp
*/
QRegion QTransformedScreen::mapToDevice(const QRegion &rgn, const QSize &s) const
{
    if (d_ptr->transformation == None)
        return QProxyScreen::mapToDevice(rgn, s);

#ifdef QT_REGION_DEBUG
    qDebug() << "mapToDevice size" << s << "rgn:  " << rgn;
#endif
    QRect tr;
    QRegion trgn;
    QVector<QRect> a = rgn.rects();
    const QRect *r = a.data();

    int w = s.width();
    int h = s.height();
    int size = a.size();

    switch (d_ptr->transformation) {
    case None:
        break;
    case Rot90:
        for (int i = 0; i < size; i++, r++) {
            tr.setCoords(r->y(), w - r->x() - 1,
                         r->bottom(), w - r->right() - 1);
            trgn |= correctNormalized(tr);
        }
        break;
    case Rot180:
        for (int i = 0; i < size; i++, r++) {
            tr.setCoords(w - r->x() - 1, h - r->y() - 1,
                         w - r->right() - 1, h - r->bottom() - 1);
            trgn |= correctNormalized(tr);
        }
        break;
    case Rot270:
        for (int i = 0; i < size; i++, r++) {
            tr.setCoords(h - r->y() - 1, r->x(),
                         h - r->bottom() - 1, r->right());
            trgn |= correctNormalized(tr);
        }
        break;
    }
#ifdef QT_REGION_DEBUG
    qDebug() << "mapToDevice trgn:  " << trgn;
#endif
    return trgn;
}

/*!
    \reimp
*/
QRegion QTransformedScreen::mapFromDevice(const QRegion &rgn, const QSize &s) const
{
    if (d_ptr->transformation == None)
        return QProxyScreen::mapFromDevice(rgn, s);

#ifdef QT_REGION_DEBUG
    qDebug() << "fromDevice: realRegion count:  " << rgn.rects().size() << " isEmpty? " << rgn.isEmpty() << "  bounds:" << rgn.boundingRect();
#endif
    QRect tr;
    QRegion trgn;
    QVector<QRect> a = rgn.rects();
    const QRect *r = a.data();

    int w = s.width();
    int h = s.height();
    int size = a.size();

    switch (d_ptr->transformation) {
    case None:
        break;
    case Rot90:
        for (int i = 0; i < size; i++, r++) {
            tr.setCoords(h - r->y() - 1, r->x(),
                         h - r->bottom() - 1, r->right());
            trgn |= correctNormalized(tr);
        }
        break;
    case Rot180:
        for (int i = 0; i < size; i++, r++) {
            tr.setCoords(w - r->x() - 1, h - r->y() - 1,
                         w - r->right() - 1, h - r->bottom() - 1);
            trgn |= correctNormalized(tr);
        }
        break;
    case Rot270:
        for (int i = 0; i < size; i++, r++) {
            tr.setCoords(r->y(), w - r->x() - 1,
                         r->bottom(), w - r->right() - 1);
            trgn |= correctNormalized(tr);
        }
        break;
    }
#ifdef QT_REGION_DEBUG
    qDebug() << "fromDevice: transRegion count: " << trgn.rects().size() << " isEmpty? " << trgn.isEmpty() << "  bounds:" << trgn.boundingRect();
#endif
    return trgn;
}

/*!
    \reimp
*/
void QTransformedScreen::setDirty(const QRect& rect)
{
    const QRect r = mapToDevice(rect, QSize(width(), height()));
    QProxyScreen::setDirty(r);
}

/*!
    \reimp
*/
QRegion QTransformedScreen::region() const
{
    QRegion deviceRegion = QProxyScreen::region();
    return mapFromDevice(deviceRegion, QSize(deviceWidth(), deviceHeight()));
}

QT_END_NAMESPACE

#endif // QT_NO_QWS_TRANSFORMED
