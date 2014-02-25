/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#ifndef __QT_IMAGE_AREA_H__
#define __QT_IMAGE_AREA_H__

#include <QLabel>
#include "DAVAEngine.h"

class QMimeData;

class ImageArea : public QLabel
{
    Q_OBJECT
    
public:
    
    enum eColorCmponents
    {
        COMPONENTS_RED = 0,
        COMPONENTS_GREEN,
        COMPONENTS_BLUE,
        COMPONENTS_ALPHA,
        COMPONENTS_ALL
        
    };
    
    ImageArea(QWidget *parent = 0, eColorCmponents value = COMPONENTS_ALL);
    ~ImageArea();
    
    void SetColorComponent(eColorCmponents value);
    
    void SetImage(const DAVA::FilePath& filePath);
    
    DAVA::Vector2 GetAcceptableSize() const;
    
public slots:
    void clear();
    void UpdatePreviewPicture();
    
    void SetAcceptableSize(const DAVA::Vector2& size);
    
signals:
    
    void changed();
    
protected:

    void mousePressEvent(QMouseEvent * event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    
    DAVA::Image* GetComponentImage(DAVA::Image* originalImage);

    void ConnectSignals();
    
    eColorCmponents colorComponent;
    DAVA::Image* image;
    DAVA::Vector2 acceptableSize;
};

#endif /* defined(__QT_IMAGE_AREA_H__) */
