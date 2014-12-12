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

#ifndef __SWR_BACKEND_H__
#define __SWR_BACKEND_H__

#include "os.h"

#include <assert.h>
#include "context.h"
#include "resource.h"

void ProcessClearBE(DRAW_CONTEXT *pDC, UINT macroTile, void *pUserData);
void storeTile(UINT x, UINT y, RENDERTARGET *pRenderTarget);
void ProcessStoreTileBE(DRAW_CONTEXT *pDC, UINT macroTile, void *pData);
void ProcessCopyBE(DRAW_CONTEXT *pDC, UINT macroTile, void *pData);

struct OS_SWAP_CHAIN
{
    virtual void Initialize(HANDLE hDisplay, HANDLE hWnd, HANDLE hVisInfo, UINT width, UINT height, UINT numBuffers) = 0;
    virtual void Destroy() = 0;
    virtual void DrawString(UINT x, UINT y, const char *string, UINT length) = 0;
    virtual void Resize(UINT width, UINT height) = 0;
    virtual void GetWindowSize(UINT *width, UINT *height) = 0;
    virtual void GetBackBuffer(void **pData, UINT *pitch) = 0;
    virtual void ReleaseBackBuffer() = 0;
    virtual void Present() = 0;
    virtual void Flip() = 0;

    // @todo DC is win only concept
    virtual HANDLE GetBackBufferDC() = 0;
    virtual void ReleaseBackBufferDC(HANDLE hdc) = 0;
};

#ifdef _WIN32
#include <dxgi.h>
#include <d3d9.h>
struct WIN_SWAP_CHAIN : OS_SWAP_CHAIN
{
    IDirect3D9 *mpD3d9;
    IDirect3DDevice9 *mpDxDevice;
    IDirect3DSurface9 *mpCurBackBuffer;
    UINT mNumBuffers;
    HWND mHWnd;

    virtual void Initialize(HANDLE hDisplay, HANDLE hWnd, HANDLE hVisInfo, UINT width, UINT height, UINT numBuffers)
    {
        mNumBuffers = numBuffers;
        mHWnd = (HWND)hWnd;
        mpDxDevice = NULL;

        mpD3d9 = Direct3DCreate9(D3D_SDK_VERSION);

        if (width == 0 || height == 0)
        {
            return;
        }

        D3DDISPLAYMODE mode = { 0 };
        mpD3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);

        D3DPRESENT_PARAMETERS params;
        ZeroMemory(&params, sizeof(params));
        params.BackBufferWidth = 0;  //mWidth;
        params.BackBufferHeight = 0; //mHeight;
        params.BackBufferFormat = D3DFMT_X8R8G8B8;
        params.BackBufferCount = mNumBuffers - 1;
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow = mHWnd;
        params.Windowed = TRUE;
        params.EnableAutoDepthStencil = FALSE;
        params.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        HRESULT hr = mpD3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, &mpDxDevice);
        assert(hr == S_OK);
    }

    virtual void Destroy()
    {
        if (mpDxDevice)
        {
            mpDxDevice->Release();
        }

        mpD3d9->Release();
    }

    virtual void GetWindowSize(UINT *width, UINT *height)
    {
        RECT rect;
        GetClientRect(mHWnd, &rect);
        *width = rect.right - rect.left;
        *height = rect.bottom - rect.top;
    }

    virtual void Resize(UINT width, UINT height)
    {
        D3DPRESENT_PARAMETERS params;
        ZeroMemory(&params, sizeof(params));
        params.BackBufferWidth = 0;  //mWidth;
        params.BackBufferHeight = 0; //mHeight;
        params.BackBufferFormat = D3DFMT_UNKNOWN;
        params.BackBufferCount = mNumBuffers - 1;
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow = mHWnd;
        params.Windowed = TRUE;
        params.EnableAutoDepthStencil = FALSE;
        params.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        HRESULT hr = mpDxDevice->Reset(&params);
    }

    virtual void GetBackBuffer(void **pData, UINT *pitch)
    {
        mpDxDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &mpCurBackBuffer);

        D3DLOCKED_RECT rect;
        HRESULT hr = mpCurBackBuffer->LockRect(&rect, NULL, D3DLOCK_DISCARD);

        if (hr == D3D_OK)
        {
            *pData = rect.pBits;
            *pitch = rect.Pitch;
        }
    }

    virtual void ReleaseBackBuffer()
    {
        assert(mpCurBackBuffer);
        if (mpCurBackBuffer)
        {
            mpCurBackBuffer->UnlockRect();
            mpCurBackBuffer->Release();
            mpCurBackBuffer = NULL;
        }
    }

    virtual void Present()
    {
        assert(mpCurBackBuffer == NULL);
        mpDxDevice->Present(NULL, NULL, 0, NULL);
    }

    virtual void Flip()
    {
    }

    virtual HANDLE GetBackBufferDC()
    {
        assert(mpCurBackBuffer == NULL);
        mpDxDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &mpCurBackBuffer);
        HDC hdc;
        mpCurBackBuffer->GetDC(&hdc);
        return hdc;
    }

    virtual void ReleaseBackBufferDC(HANDLE hdc)
    {
        assert(mpCurBackBuffer);
        mpCurBackBuffer->ReleaseDC((HDC)hdc);
        mpCurBackBuffer->Release();
        mpCurBackBuffer = NULL;
    }

    virtual void DrawString(UINT x, UINT y, const char *string, UINT length)
    {
        HDC hdc = (HDC)GetBackBufferDC();
        SetBkColor(hdc, RGB(0, 0, 0));
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, OPAQUE);
        TextOut(hdc, x, y, string, length);
        ReleaseBackBufferDC(hdc);
    }
};

struct SWAP_CHAIN
{
    SWR_CONTEXT *mContext;

    RENDERTARGET **mBuffers;
    RENDERTARGET *mDepthBuffer;

    UINT mNumBuffers;
    UINT mFrontBufferIndex;
    UINT mWidth, mHeight;
    OS_SWAP_CHAIN *mpOSSwapChain;

    void Initialize(SWR_CONTEXT *context, HANDLE hDisplay, HANDLE hWnd, HANDLE hVisInfo, UINT width, UINT height, UINT numBuffers)
    {
        mpOSSwapChain = new WIN_SWAP_CHAIN();
        mContext = context;
        mNumBuffers = numBuffers;

        mBuffers = (RENDERTARGET **)malloc(sizeof(RENDERTARGET *) * numBuffers);
        for (UINT i = 0; i < numBuffers; ++i)
        {
            mBuffers[i] = (RENDERTARGET *)CreateRenderTarget(mContext, width, height, BGRA8_UNORM);
        }
        mDepthBuffer = CreateRenderTarget(mContext, width, height, R32_FLOAT);

        mWidth = width;
        mHeight = height;
        mFrontBufferIndex = 0;

        mpOSSwapChain->Initialize(hDisplay, hWnd, hVisInfo, width, height, numBuffers);
    }

    void Destroy()
    {
        for (UINT i = 0; i < mNumBuffers; ++i)
        {
            DestroyRenderTarget(mContext, mBuffers[i]);
        }

        DestroyRenderTarget(mContext, mDepthBuffer);

        mpOSSwapChain->Destroy();
        delete mpOSSwapChain;
    }

    void Resize(UINT width, UINT height)
    {
        // Nothing to do with width/height hasn't changed
        if (width == mWidth && height == mHeight)
        {
            return;
        }

        // Only resize if the underlying window changed
        UINT winw, winh;
        mpOSSwapChain->GetWindowSize(&winw, &winh);
        if (winw != mWidth || winh != mHeight)
        {
            // wait for all rendering to complete
            WaitForDependencies(mContext, mContext->nextDrawId - 1);

            for (UINT i = 0; i < mNumBuffers; ++i)
            {
                DestroyRenderTarget(mContext, mBuffers[i]);
            }
            DestroyRenderTarget(mContext, mDepthBuffer);

            for (UINT i = 0; i < mNumBuffers; ++i)
            {
                mBuffers[i] = (RENDERTARGET *)CreateRenderTarget(mContext, winw, winh, BGRA8_UNORM);
            }
            mDepthBuffer = CreateRenderTarget(mContext, winw, winh, R32_FLOAT);

            mWidth = winw;
            mHeight = winh;

            mpOSSwapChain->Resize(winw, winh);
        }
    }

    RENDERTARGET *GetFrontBuffer()
    {
        return mBuffers[mFrontBufferIndex];
    }

    RENDERTARGET *GetBackBuffer(UINT index)
    {
        assert(index < mNumBuffers - 1);
        UINT backBuffer = (mFrontBufferIndex + index + 1) % mNumBuffers;
        return mBuffers[backBuffer];
    }

    RENDERTARGET *GetDepthBuffer()
    {
        return mDepthBuffer;
    }

    // Must be released with ReleaseOSBackBuffer()
    void GetOSBackBuffer(void **pData, UINT *pitch)
    {
        mpOSSwapChain->GetBackBuffer(pData, pitch);
    }

    void ReleaseOSBackBuffer()
    {
        mpOSSwapChain->ReleaseBackBuffer();
    }

    //@todo WIN only concept.  how to expose
    HANDLE GetBackBufferDC()
    {
        return mpOSSwapChain->GetBackBufferDC();
    }

    void ReleaseBackBufferDC(HANDLE hdc)
    {
        mpOSSwapChain->ReleaseBackBufferDC(hdc);
    }

    void Present()
    {
        mpOSSwapChain->Present();
    }

    void Flip()
    {
        mFrontBufferIndex = (mFrontBufferIndex + 1) % mNumBuffers;
        mpOSSwapChain->Flip();
    }

    void DrawString(UINT x, UINT y, const char *string, UINT length)
    {
        mpOSSwapChain->DrawString(x, y, string, length);
    }
};
#endif

#endif //__SWR_BACKEND_H__
