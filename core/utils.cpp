// Copyright 2014 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if defined(_WIN32)

#include <Windows.h>
#include <Gdiplus.h>
#include <Gdiplusheaders.h>

using namespace Gdiplus;

int GetEncoderClsid(const WCHAR *format, CLSID *pClsid)
{
    UINT num = 0;  // number of image encoders
    UINT size = 0; // size of the image encoder array in bytes

    ImageCodecInfo *pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1; // Failure

    pImageCodecInfo = (ImageCodecInfo *)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1; // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j; // Success
        }
    }

    free(pImageCodecInfo);
    return -1; // Failure
}

void SaveBitmapToFile(
    const WCHAR *pFilename,
    void *pBuffer,
    UINT width,
    UINT height)
{
    // dump pixels to a bmp
    // Initialize GDI+.
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    Bitmap *bitmap = new Bitmap(width, height);
    BYTE *pBytes = (BYTE *)pBuffer;
    static const UINT bytesPerPixel = 4;
    for (UINT y = 0; y < height; ++y)
        for (UINT x = 0; x < width; ++x)
        {
            UINT pixel = *(UINT *)pBytes;
            Color color(pixel);
            bitmap->SetPixel(x, y, color);
            pBytes += bytesPerPixel;
        }

    // Save image.
    CLSID pngClsid;
    GetEncoderClsid(L"image/png", &pngClsid);
    bitmap->Save(pFilename, &pngClsid, NULL);

    delete bitmap;

    GdiplusShutdown(gdiplusToken);
}

void OpenBitmapFromFile(
    const WCHAR *pFilename,
    void **pBuffer,
    UINT *width,
    UINT *height)
{
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    Bitmap *bitmap = new Bitmap(pFilename);

    *width = bitmap->GetWidth();
    *height = bitmap->GetHeight();
    *pBuffer = new BYTE[*width * *height * 4]; // width * height * |RGBA|

    // The folder 'stb_image' contains a PNG open/close module which
    // is far less painful than this is, yo.
    Gdiplus::Color clr;
    for (UINT y = 0, idx = 0; y < *height; ++y)
    {
        for (UINT x = 0; x < *width; ++x, idx += 4)
        {
            bitmap->GetPixel(x, *height - y - 1, &clr);
            ((BYTE *)*pBuffer)[idx + 0] = clr.GetBlue();
            ((BYTE *)*pBuffer)[idx + 1] = clr.GetGreen();
            ((BYTE *)*pBuffer)[idx + 2] = clr.GetRed();
            ((BYTE *)*pBuffer)[idx + 3] = clr.GetAlpha();
        }
    }

    delete bitmap;
    bitmap = 0;
}
#endif
