#include "precomp.h"

void CopyImageToClipboard (IplImage* img) {
	if ((img==NULL) || (img->nChannels != 3)) return;
	if (::OpenClipboard(NULL)) {

		// create HGLOBAL storage for image
		HGLOBAL  hMem = GlobalAlloc (GMEM_MOVEABLE, img->imageSize + sizeof(BITMAPINFOHEADER) + 16);
		BYTE *pClipData = (BYTE*)GlobalLock(hMem);

		// fill in image info
		BITMAPINFOHEADER bmh;
		ZeroMemory(&bmh, sizeof(BITMAPINFOHEADER));
		bmh.biBitCount = 24;
		bmh.biPlanes = 1;
		bmh.biHeight = -img->height;
		bmh.biWidth = img->width;
		bmh.biSize = sizeof(BITMAPINFOHEADER);
		bmh.biSizeImage = img->imageSize;

		CopyMemory (pClipData, &bmh, sizeof(BITMAPINFOHEADER)) ;
		pClipData += sizeof(BITMAPINFOHEADER);

		CopyMemory (pClipData, img->imageData, img->imageSize) ;
		GlobalUnlock (hMem) ;
		if (::EmptyClipboard()) {
			::SetClipboardData (CF_DIB, hMem) ;
		} else {
			GlobalFree (hMem) ;  // something went wrong, so we better free the memory
		}
		::CloseClipboard() ;
		// GlobalFree (hMem) ; // Clipboard now owns the memory location, so we don't free it!
	}
}

void IplToBitmap(IplImage *src, Bitmap *dst) {
    BitmapData bmData;
    bmData.Width = src->width;
    bmData.Height = src->height;
    bmData.PixelFormat = PixelFormat24bppRGB;
    bmData.Stride = src->widthStep;
    bmData.Scan0 = src->imageData;
    Rect sampleRect(0, 0, src->width, src->height);
    dst->LockBits(&sampleRect, ImageLockModeWrite | ImageLockModeUserInputBuf, PixelFormat24bppRGB, &bmData);
    dst->UnlockBits(&bmData);
}

CvScalar hsv2rgb( float hue ) {
    int rgb[3], p, sector;
    static const int sector_data[][3]=
        {{0,2,1}, {1,2,0}, {1,0,2}, {2,0,1}, {2,1,0}, {0,1,2}};
    hue *= 0.033333333333333333333333333333333f;
    sector = cvFloor(hue);
    p = cvRound(255*(hue - sector));
    p ^= sector & 1 ? 255 : 0;

    rgb[sector_data[sector][0]] = 255;
    rgb[sector_data[sector][1]] = 0;
    rgb[sector_data[sector][2]] = p;

    return cvScalar(rgb[2], rgb[1], rgb[0],0);
}

CvScalar colorSwatch[COLOR_SWATCH_SIZE] = {
    CV_RGB(0x45,0x8A,0x8A),
    CV_RGB(0xE6,0xA1,0x73),
    CV_RGB(0xE6,0xC3,0x73),
    CV_RGB(0x5C,0x5C,0xA1),
    CV_RGB(0x40,0x80,0x80),
    CV_RGB(0x80,0x59,0x40),
    CV_RGB(0x80,0x6C,0x40),
    CV_RGB(0x49,0x49,0x80),
    CV_RGB(0xCF,0xE6,0xE6),
    CV_RGB(0xE6,0xD8,0xCF),
    CV_RGB(0xE6,0xDF,0xCF),
    CV_RGB(0xD2,0xD2,0xE6),
    CV_RGB(0x30,0xBF,0xBF),
    CV_RGB(0xBF,0x69,0x30),
    CV_RGB(0xBF,0x94,0x30),
    CV_RGB(0x44,0x44,0xBF)
};

WCHAR *filterNames[] = { L"Color", L"Shape", L"Brightness", L"SIFT",
							 L"Adaboost", L"Motion", L"Gesture" };

void DrawArrow(IplImage *img, CvPoint center, double angleDegrees, double magnitude, CvScalar color, int thickness) {
	CvPoint endpoint, arrowpoint;
    double angle = angleDegrees*CV_PI/180.0;
	endpoint.x = (int) (center.x + magnitude*cos(angle));
	endpoint.y = (int) (center.y + magnitude*sin(angle));

	/* Draw the main line of the arrow. */
	cvLine(img, center, endpoint, color, thickness, CV_AA, 0);
	/* Now draw the tips of the arrow, scaled to the size of the main part*/			
	arrowpoint.x = (int) (endpoint.x + 12* cos(angle + 3*CV_PI/4));
	arrowpoint.y = (int) (endpoint.y + 12* sin(angle + 3*CV_PI/4));
	cvLine(img, arrowpoint, endpoint, color, thickness, CV_AA, 0);
	arrowpoint.x = (int) (endpoint.x + 12* cos(angle - 3*CV_PI/4));
	arrowpoint.y = (int) (endpoint.y + 12* sin(angle - 3*CV_PI/4));
	cvLine(img, arrowpoint, endpoint, color, thickness, CV_AA, 0);
}

void DrawTrack(IplImage *img, MotionTrack mt,  CvScalar color, int thickness, float squareSize, int maxPointsToDraw) {
	int nPoints = (int) mt.size();
	int startPoint = 0;
	if (maxPointsToDraw > 0) {
		if (nPoints > maxPointsToDraw) {
			startPoint = nPoints - maxPointsToDraw;
			nPoints = maxPointsToDraw;
		}
	}
    CvPoint *trackPoints = new CvPoint[nPoints];
    for (int i=0; i<nPoints; i++) {
        trackPoints[i].x = (int) (img->width/2 + (mt[i+startPoint].m_x*img->width)/squareSize);
        trackPoints[i].y = (int) (img->height/2 + (mt[i+startPoint].m_y*img->height)/squareSize);
    }
    cvPolyLine(img, &trackPoints, &nPoints, 1, 0, color, thickness, CV_AA);
    delete[] trackPoints;
}

void DrawTrack(Graphics *graphics, MotionTrack mt, float width, float height, float squareSize) {
	int nPoints = (int) mt.size();
	PointF *trackPoints = new PointF[nPoints];
    for (int i = 0; i<nPoints; i++) {
        trackPoints[i].X = (REAL) (width/2 + (mt[i].m_x*width)/squareSize);
        trackPoints[i].Y = (REAL) (height/2 + (mt[i].m_y*height)/squareSize);
	}
    for (int i = 0; i<nPoints-1; i++) {
		Pen p(Color((255*i)/nPoints, 100,255,100), 4);
	    p.SetEndCap(LineCapRound);
	    graphics->DrawLine(&p, trackPoints[i], trackPoints[i+1]);
	}
    delete[] trackPoints;
}

bool DeleteDirectory(LPCTSTR lpszDir, bool useRecycleBin = true) {
    int len = (int) _tcslen(lpszDir);
    TCHAR *pszFrom = new TCHAR[len+2];
    _tcscpy(pszFrom, lpszDir);
    pszFrom[len] = 0;
    pszFrom[len+1] = 0;

    SHFILEOPSTRUCT fileop;
    fileop.hwnd   = NULL;    // no status display
    fileop.wFunc  = FO_DELETE;  // delete operation
    fileop.pFrom  = pszFrom;  // source file name as double null terminated string
    fileop.pTo    = NULL;    // no destination needed
    fileop.fFlags = FOF_NOCONFIRMATION|FOF_SILENT;  // do not prompt the user

    if (useRecycleBin) {
        fileop.fFlags |= FOF_ALLOWUNDO;
    }

    fileop.fAnyOperationsAborted = FALSE;
    fileop.lpszProgressTitle     = NULL;
    fileop.hNameMappings         = NULL;

    int ret = SHFileOperation(&fileop);
    delete [] pszFrom;  
    return (ret == 0);
}
