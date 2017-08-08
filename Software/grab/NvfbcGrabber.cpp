/*
* NvfbcGrabber.hpp
*
*  Created on: 06.03.17
*     Project: Lightpack
*
*  Copyright (c) 2017
*
*  Lightpack a USB content-driving ambient lighting system
*
*  Lightpack is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 2 of the License, or
*  (at your option) any later version.
*
*  Lightpack is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#ifdef NVFBC_GRAB_SUPPORT

#include "debug.h"
#include "NvfbcGrabber.hpp"
#include "GrabberContext.hpp"

#include <NvFBCLibrary.h>
#include <NvFBC/NvFBC.h>
#include <NvFBC/nvFBCToSys.h>

#include <QApplication>
#include <QDesktopWidget>

QRect operator*(const QRect& rect, double scalar)
{
    QRect ret(rect.x() * scalar, rect.y() * scalar, rect.width() * scalar, rect.height() * scalar);
    return ret;
}

QRect operator/(const QRect& rect, double scalar) 
{
    QRect ret(rect.x() / scalar, rect.y() / scalar, rect.width() / scalar, rect.height() / scalar);
    return ret;
}


NvfbcGrabber::NvfbcGrabber(QObject* parent, GrabberContext* context)
    : GrabberBase(parent, context), m_nvfbc(NULL), m_pNvfbcToSys(NULL), m_frameBuffer(NULL) {
    m_nvfbc = new NvFBCLibrary;
    NvFBCLibrary& nvfbc = *static_cast<NvFBCLibrary*>(m_nvfbc);

    NVFBCRESULT fbcRes = NVFBC_SUCCESS;

    // Load the nvfbc Library.
    if (!nvfbc.load()) {
        qWarning() << "Unable to load the NvFBC library";
        return;
    }

    // Get status.
    NvFBCStatusEx status = { 0 };
    status.dwVersion = NVFBC_STATUS_VER;
    fbcRes = nvfbc.getStatus(&status);

    // Create NVFBC connection.
    // One connection is possible per display head. This grabber only uses the primary screen for now.
    NvFBCCreateParams createParams = { 0 };
    createParams.dwVersion = NVFBC_CREATE_PARAMS_VER;
    createParams.dwInterfaceType = NVFBC_TO_SYS;
    //createParams.dwPrivateDataSize = ??
    //createParams.pPrivateData = ??

    // Create an instance of the NvFBCToSys class, the first argument specifies the frame buffer.
    fbcRes = nvfbc.createEx(&createParams);

    if (fbcRes != NVFBC_SUCCESS || createParams.pNvFBC == NULL) {
        qWarning() << "Failed to create an instance of NvFBCToSys";
        return;
    }

    m_pNvfbcToSys = createParams.pNvFBC;
    NvFBCToSys& nvfbcToSys = *static_cast<NvFBCToSys*>(m_pNvfbcToSys);

    // Setup the frame grab.
    NVFBC_TOSYS_SETUP_PARAMS fbcSysSetupParams = { 0 };
    fbcSysSetupParams.dwVersion = NVFBC_TOSYS_SETUP_PARAMS_VER;
    fbcSysSetupParams.eMode = NVFBC_TOSYS_ARGB;
    fbcSysSetupParams.ppBuffer = reinterpret_cast<void**>(&m_frameBuffer);

    fbcRes = nvfbcToSys.NvFBCToSysSetUp(&fbcSysSetupParams);
    if (fbcRes == NVFBC_SUCCESS) {
        qWarning() << "Failed to setup NvFBCToSys. NvFBCToSysSetup returned: " << fbcRes;
        return;
    }
}

NvfbcGrabber::~NvfbcGrabber() 
{
    // Release the NvFBCToSys object.
    NvFBCToSys& nvfbcToSys = *static_cast<NvFBCToSys*>(m_pNvfbcToSys);
    nvfbcToSys.NvFBCToSysRelease();

    delete static_cast<NvFBCLibrary *>(m_nvfbc);
}

GrabResult NvfbcGrabber::grabScreens() 
{
    foreach(GrabbedScreen grabScreen, _screensWithWidgets) {
        QRect scaledScreenRect = grabScreen.screenInfo.rect;

        NVFBCRESULT fbcRes = NVFBC_SUCCESS;
        NvFBCToSys& nvfbcToSys = *static_cast<NvFBCToSys*>(m_pNvfbcToSys);
        NvFBCFrameGrabInfo frameGrabInfo = { 0 };

        NVFBC_TOSYS_GRAB_FRAME_PARAMS fbcSysGrabParams = { 0 };
        fbcSysGrabParams.dwVersion = NVFBC_TOSYS_GRAB_FRAME_PARAMS_VER;
        fbcSysGrabParams.dwFlags = NVFBC_TOSYS_NOFLAGS;
        fbcSysGrabParams.eGMode = NVFBC_TOSYS_SOURCEMODE_SCALE;
        fbcSysGrabParams.dwTargetWidth = scaledScreenRect.width();
        fbcSysGrabParams.dwTargetHeight = scaledScreenRect.height();
        fbcSysGrabParams.pNvFBCFrameGrabInfo = &frameGrabInfo;

        fbcRes = nvfbcToSys.NvFBCToSysGrabFrame(&fbcSysGrabParams);
        // Image watch: @mem(m_frameBuffer, UINT8, 4, <w>, <h>, <w> * 4)
        if (fbcRes != NVFBC_SUCCESS) {
            qWarning() << "Grab frame failed. NvFBCToSysGrabFrame returned: " << fbcRes;
            return GrabResultError;
        }
    }
    return GrabResultOk;
}


bool NvfbcGrabber::reallocate(const QList<ScreenInfo>& screens)
{
    // Grabbing a downscaled image for low performance impact.
    // As a rule of thumb, downscale each side by a factor of four
    // by default, or a factor of eight for resolutions like QHD or 4K.
    int scaling = 4;
    QRect rect = QApplication::desktop()->screenGeometry(QApplication::desktop()->primaryScreen());
    if (rect.width() > 2048)
        scaling = 8;

    for (int i = 0; i < screens.size(); ++i) {
        const ScreenInfo screen = screens[i];
        ScreenInfo scaledScreen = screen;
        scaledScreen.rect = screen.rect / scaling;
        // Handle stores scaling factor
        scaledScreen.handle = reinterpret_cast<void*>(scaling); 

        // Do a sample grab.
        NVFBCRESULT fbcRes = NVFBC_SUCCESS;
        NvFBCToSys& nvfbcToSys = *static_cast<NvFBCToSys*>(m_pNvfbcToSys);
        NvFBCFrameGrabInfo frameGrabInfo = { 0 };

        NVFBC_TOSYS_GRAB_FRAME_PARAMS fbcSysGrabParams = { 0 };
        fbcSysGrabParams.dwVersion = NVFBC_TOSYS_GRAB_FRAME_PARAMS_VER;
        fbcSysGrabParams.dwFlags = NVFBC_TOSYS_NOWAIT;
        fbcSysGrabParams.eGMode = NVFBC_TOSYS_SOURCEMODE_SCALE;
        fbcSysGrabParams.dwTargetWidth = scaledScreen.rect.width();
        fbcSysGrabParams.dwTargetHeight = scaledScreen.rect.height();
        fbcSysGrabParams.pNvFBCFrameGrabInfo = &frameGrabInfo;

        fbcRes = nvfbcToSys.NvFBCToSysGrabFrame(&fbcSysGrabParams);
        Q_ASSERT(scaledScreen.rect.width() == frameGrabInfo.dwWidth);
        Q_ASSERT(scaledScreen.rect.height() == frameGrabInfo.dwHeight);

        GrabbedScreen grabbedScreen;
        grabbedScreen.imgDataSize = frameGrabInfo.dwBufferWidth * frameGrabInfo.dwHeight * 4;
        grabbedScreen.imgData = static_cast<unsigned char*>(m_frameBuffer);
        grabbedScreen.imgFormat = BufferFormatArgb;
        grabbedScreen.screenInfo = scaledScreen;

        _screensWithWidgets.append(grabbedScreen);
    }

    // Apply downscaling to grabwidgets
    for (int i = 0; i < _context->grabWidgets->size(); ++i) {
        GrabWidget* widget = _context->grabWidgets->at(i);
        widget->setGeometry(widget->frameGeometry() / scaling);
    }

    return true;
}


QList<ScreenInfo>* NvfbcGrabber::screensWithWidgets(QList<ScreenInfo>* result, const QList<GrabWidget*>& grabWidgets) 
{
    result->clear();

    ScreenInfo screenInfo;
    screenInfo.rect = QApplication::desktop()->screenGeometry(QApplication::desktop()->primaryScreen());
    // Handle stores scaling factor
    screenInfo.handle = reinterpret_cast<void*>(1); 
    result->append(screenInfo);
    return result;
}

bool NvfbcGrabber::isReallocationNeeded(const QList<ScreenInfo>& screens) const
{
    if (_screensWithWidgets.size() == 0 || screens.size() != _screensWithWidgets.size())
        return true;

    for (int i = 0; i < screens.size(); ++i) {
        ScreenInfo screenInfo = _screensWithWidgets[i].screenInfo;
        // Handle stores scaling factor
        if (screens[i].rect * reinterpret_cast<int>(screens[i].handle) != 
            screenInfo.rect * reinterpret_cast<int>(screenInfo.handle))
                return true;
    }
    return false;
}

#endif
