#include "pch.h"

#include "log.h"
#include "sw_render.h"

#include <cmath>

namespace
{
    // 32bpp premultiplied DIB，负高度 = top-down，内存字节序 B,G,R,A。
    struct SwBitmap
    {
        HDC dc = nullptr;
        HBITMAP bitmap = nullptr;
        HBITMAP oldBitmap = nullptr;
        void* bits = nullptr;
        int width = 0;
        int height = 0;
    };

    SwBitmap g_bitmap;
    bool g_blendBroken = false; // GdiAlphaBlend 不可用时降级 BitBlt 只提示一次

    using AlphaBlendFn = BOOL(WINAPI*)(
        HDC, LONG, LONG, LONG, LONG, HDC, LONG, LONG, LONG, LONG, BLENDFUNCTION);
    AlphaBlendFn g_alphaBlend = nullptr;

    void EnsureAlphaBlend()
    {
        if (g_alphaBlend)
        {
            return;
        }
        const HMODULE gdi32 = GetModuleHandleW(L"gdi32.dll");
        if (gdi32)
        {
            g_alphaBlend = reinterpret_cast<AlphaBlendFn>(
                GetProcAddress(gdi32, "GdiAlphaBlend"));
        }
        if (!g_alphaBlend)
        {
            // 老系统兜底：msimg32!AlphaBlend（标准导出）
            const HMODULE msimg32 = LoadLibraryW(L"msimg32.dll");
            if (msimg32)
            {
                g_alphaBlend = reinterpret_cast<AlphaBlendFn>(
                    GetProcAddress(msimg32, "AlphaBlend"));
            }
        }
        if (!g_alphaBlend)
        {
            Ra2Overlay::Log::Write("SoftwareRender: neither GdiAlphaBlend nor msimg32!AlphaBlend available");
        }
    }

    bool EnsureBitmap(int width, int height)
    {
        if (g_bitmap.dc && g_bitmap.width == width && g_bitmap.height == height)
        {
            return true;
        }

        if (g_bitmap.dc)
        {
            SelectObject(g_bitmap.dc, g_bitmap.oldBitmap);
            DeleteObject(g_bitmap.bitmap);
            DeleteDC(g_bitmap.dc);
            g_bitmap = SwBitmap{};
        }

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height; // top-down
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        g_bitmap.bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &g_bitmap.bits, nullptr, 0);
        if (!g_bitmap.bitmap || !g_bitmap.bits)
        {
            Ra2Overlay::Log::Write("SoftwareRender: CreateDIBSection failed (%d x %d)", width, height);
            g_bitmap.bitmap = nullptr;
            return false;
        }

        g_bitmap.dc = CreateCompatibleDC(nullptr);
        if (!g_bitmap.dc)
        {
            Ra2Overlay::Log::Write("SoftwareRender: CreateCompatibleDC failed");
            DeleteObject(g_bitmap.bitmap);
            g_bitmap.bitmap = nullptr;
            return false;
        }
        g_bitmap.oldBitmap = static_cast<HBITMAP>(
            SelectObject(g_bitmap.dc, g_bitmap.bitmap));
        g_bitmap.width = width;
        g_bitmap.height = height;
        return true;
    }

    inline float EdgeCross(float ax, float ay, float bx, float by, float px, float py)
    {
        // (b-a) x (p-a) 的 z 分量：>0 表示 p 在 a->b 左侧
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    }

    void FillTriangle(
        uint32_t* dst,
        int dstWidth,
        int dstHeight,
        const ImDrawVert& va,
        const ImDrawVert& vbIn,
        const ImDrawVert& vcIn,
        ImTextureData* tex,
        const ImVec4& clip)
    {
        // 归一化绕向：保证 area2 > 0（CCW），边函数全部 >= 0 即内部
        float area2 = (vbIn.pos.x - va.pos.x) * (vcIn.pos.y - va.pos.y)
            - (vbIn.pos.y - va.pos.y) * (vcIn.pos.x - va.pos.x);
        if (area2 < 0.0001f && area2 > -0.0001f)
        {
            return;
        }
        const ImDrawVert* vb = &vbIn;
        const ImDrawVert* vc = &vcIn;
        if (area2 < 0.0f)
        {
            ImDrawVert tmp = *vb;
            vb = vc;
            vc = &tmp;
            area2 = -area2;
        }
        const float invArea = 1.0f / area2;

        const float minX = (std::min)({ va.pos.x, vb->pos.x, vc->pos.x, clip.x });
        const float minY = (std::min)({ va.pos.y, vb->pos.y, vc->pos.y, clip.y });
        const float maxX = (std::min)({ va.pos.x, vb->pos.x, vc->pos.x, clip.z });
        const float maxY = (std::min)({ va.pos.y, vb->pos.y, vc->pos.y, clip.w });
        int x0 = (std::max)(0, static_cast<int>(std::floor(minX)));
        int y0 = (std::max)(0, static_cast<int>(std::floor(minY)));
        int x1 = (std::min)(dstWidth - 1, static_cast<int>(std::ceil(maxX)));
        int y1 = (std::min)(dstHeight - 1, static_cast<int>(std::ceil(maxY)));
        if (x0 > x1 || y0 > y1)
        {
            return;
        }

        // 纹理采样参数
        const unsigned char* texPixels = nullptr;
        int texWidth = 0;
        int texHeight = 0;
        int texBpp = 0;
        int texPitch = 0;
        if (tex && tex->Pixels)
        {
            texPixels = static_cast<const unsigned char*>(tex->Pixels);
            texWidth = tex->Width;
            texHeight = tex->Height;
            texBpp = tex->BytesPerPixel;
            texPitch = tex->GetPitch();
        }

        for (int y = y0; y <= y1; ++y)
        {
            const float py = y + 0.5f;
            for (int x = x0; x <= x1; ++x)
            {
                const float px = x + 0.5f;
                // w0/w1/w2 分别对应 va/vb/vc 的重心权重
                const float w0 = EdgeCross(vb->pos.x, vb->pos.y, vc->pos.x, vc->pos.y, px, py);
                if (w0 < 0.0f)
                {
                    continue;
                }
                const float w1 = EdgeCross(vc->pos.x, vc->pos.y, va.pos.x, va.pos.y, px, py);
                if (w1 < 0.0f)
                {
                    continue;
                }
                const float w2 = EdgeCross(va.pos.x, va.pos.y, vb->pos.x, vb->pos.y, px, py);
                if (w2 < 0.0f)
                {
                    continue;
                }
                const float l0 = w0 * invArea;
                const float l1 = w1 * invArea;
                const float l2 = w2 * invArea;

                // 插值顶点色（0..255 straight alpha）
                const float cr = (va.col & 0xFF) * l0 + (vb->col & 0xFF) * l1 + (vc->col & 0xFF) * l2;
                const float cg = ((va.col >> 8) & 0xFF) * l0 + ((vb->col >> 8) & 0xFF) * l1 + ((vc->col >> 8) & 0xFF) * l2;
                const float cb = ((va.col >> 16) & 0xFF) * l0 + ((vb->col >> 16) & 0xFF) * l1 + ((vc->col >> 16) & 0xFF) * l2;
                const float ca = ((va.col >> 24) & 0xFF) * l0 + ((vb->col >> 24) & 0xFF) * l1 + ((vc->col >> 24) & 0xFF) * l2;
                if (ca <= 0.5f)
                {
                    continue;
                }

                float tr = 255.0f, tg = 255.0f, tb = 255.0f, ta = 255.0f;
                if (texPixels)
                {
                    float u = va.uv.x * l0 + vb->uv.x * l1 + vc->uv.x * l2;
                    float v = va.uv.y * l0 + vb->uv.y * l1 + vc->uv.y * l2;
                    int tx = static_cast<int>(u);
                    int ty = static_cast<int>(v);
                    tx = (std::min)((std::max)(tx, 0), texWidth - 1);
                    ty = (std::min)((std::max)(ty, 0), texHeight - 1);
                    const unsigned char* tp = texPixels + ty * texPitch + tx * texBpp;
                    if (texBpp == 4)
                    {
                        tb = tp[0]; // ImGui 纹理按 RGBA 内存序
                        tg = tp[1];
                        tr = tp[2];
                        ta = tp[3];
                    }
                    else
                    {
                        ta = tp[0];
                    }
                }

                // 源色 = 顶点色 x 纹理（straight alpha），再转 premultiplied
                const float sr = cr * tr * (1.0f / 255.0f);
                const float sg = cg * tg * (1.0f / 255.0f);
                const float sb = cb * tb * (1.0f / 255.0f);
                const float sa = ca * ta * (1.0f / 255.0f);

                uint32_t& out = dst[y * dstWidth + x];
                if (sa >= 254.5f)
                {
                    // 不透明快路径：无需读取目标像素
                    const int pR = static_cast<int>(sr * sa * (1.0f / 255.0f));
                    const int pG = static_cast<int>(sg * sa * (1.0f / 255.0f));
                    const int pB = static_cast<int>(sb * sa * (1.0f / 255.0f));
                    out = 0xFF000000u
                        | (static_cast<uint32_t>(pR > 255 ? 255 : pR) << 16)
                        | (static_cast<uint32_t>(pG > 255 ? 255 : pG) << 8)
                        | static_cast<uint32_t>(pB > 255 ? 255 : pB);
                }
                else
                {
                    const float k = 1.0f - sa * (1.0f / 255.0f);
                    const int pR = static_cast<int>(sr * sa * (1.0f / 255.0f));
                    const int pG = static_cast<int>(sg * sa * (1.0f / 255.0f));
                    const int pB = static_cast<int>(sb * sa * (1.0f / 255.0f));
                    const int dR = static_cast<int>((out >> 16) & 0xFF);
                    const int dG = static_cast<int>((out >> 8) & 0xFF);
                    const int dB = static_cast<int>(out & 0xFF);
                    const int dA = static_cast<int>((out >> 24) & 0xFF);
                    int oR = pR + static_cast<int>(dR * k);
                    int oG = pG + static_cast<int>(dG * k);
                    int oB = pB + static_cast<int>(dB * k);
                    int oA = static_cast<int>(sa) + static_cast<int>(dA * k);
                    oR = (std::min)(oR, 255);
                    oG = (std::min)(oG, 255);
                    oB = (std::min)(oB, 255);
                    oA = (std::min)(oA, 255);
                    out = (static_cast<uint32_t>(oA) << 24)
                        | (static_cast<uint32_t>(oR) << 16)
                        | (static_cast<uint32_t>(oG) << 8)
                        | static_cast<uint32_t>(oB);
                }
            }
        }
    }
}

bool Ra2Overlay::SoftwareRender::RenderDrawData(ImDrawData* drawData, HDC targetDc)
{
    if (!drawData || !drawData->Valid || !targetDc)
    {
        return false;
    }

    const int width = static_cast<int>(drawData->DisplaySize.x);
    const int height = static_cast<int>(drawData->DisplaySize.y);
    if (width <= 0 || height <= 0)
    {
        return false;
    }
    if (!EnsureBitmap(width, height))
    {
        return false;
    }

    EnsureAlphaBlend();

    // 透明黑清屏（premultiplied 语义下即全透明）
    memset(g_bitmap.bits, 0, static_cast<size_t>(width) * height * 4);
    uint32_t* dst = static_cast<uint32_t*>(g_bitmap.bits);

    const ImVec2 displayPos = drawData->DisplayPos;
    for (ImDrawList* drawList : drawData->CmdLists)
    {
        const ImDrawVert* vtx = drawList->VtxBuffer.Data;
        const ImDrawIdx* idx = drawList->IdxBuffer.Data;
        for (int cmdIndex = 0; cmdIndex < drawList->CmdBuffer.Size; ++cmdIndex)
        {
            const ImDrawCmd& cmd = drawList->CmdBuffer[cmdIndex];
            if (cmd.ElemCount == 0 || cmd.UserCallback)
            {
                continue;
            }
            ImVec4 clip = cmd.ClipRect;
            clip.x -= displayPos.x;
            clip.y -= displayPos.y;
            clip.z -= displayPos.x;
            clip.w -= displayPos.y;
            ImTextureData* tex = cmd.TexRef._TexData;
            for (unsigned int i = 0; i + 2 < cmd.ElemCount; i += 3)
            {
                const ImDrawVert& a = vtx[cmd.VtxOffset + idx[cmd.IdxOffset + i + 0]];
                const ImDrawVert& b = vtx[cmd.VtxOffset + idx[cmd.IdxOffset + i + 1]];
                const ImDrawVert& c = vtx[cmd.VtxOffset + idx[cmd.IdxOffset + i + 2]];
                FillTriangle(dst, width, height, a, b, c, tex, clip);
            }
        }
    }

    if (g_alphaBlend)
    {
        const BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        if (g_alphaBlend(
                targetDc, 0, 0, width, height,
                g_bitmap.dc, 0, 0, width, height, blend))
        {
            return true;
        }
        if (!g_blendBroken)
        {
            g_blendBroken = true;
            Ra2Overlay::Log::Write("SoftwareRender: GdiAlphaBlend failed (err=%lu); falling back to BitBlt", GetLastError());
        }
    }

    // 兜底：不透明覆盖（不应发生，仅保底不崩）
    return BitBlt(targetDc, 0, 0, width, height, g_bitmap.dc, 0, 0, SRCCOPY) != 0;
}
