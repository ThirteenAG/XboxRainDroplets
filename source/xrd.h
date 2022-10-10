#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9tex.h>
#include "d3dvtbl.h"
#pragma comment(lib, "D3dx9.lib")
#include <time.h>
#include <injector\injector.hpp>
#include <injector\hooking.hpp>
#include <injector\calling.hpp>
#include <injector\assembly.hpp>
#include <injector\utility.hpp>
#include <algorithm>
#include <thread>
#include <mutex>
#include <map>
#include <iomanip>
#include <subauth.h>
#include "Hooking.Patterns.h"

#ifndef MACRO_START
#define MACRO_START do
#endif /* MACRO_START */

#ifndef MACRO_STOP
#define MACRO_STOP while(0)
#endif /* MACRO_STOP */

#define RwV3dSub(o, a, b)                                       \
MACRO_START                                                     \
{                                                               \
    (o)->x = (((a)->x) - ( (b)->x));                            \
    (o)->y = (((a)->y) - ( (b)->y));                            \
    (o)->z = (((a)->z) - ( (b)->z));                            \
}                                                               \
MACRO_STOP

#define RwV3dScale(o, a, s)                                     \
MACRO_START                                                     \
{                                                               \
    (o)->x = (((a)->x) * ( (s)));                               \
    (o)->y = (((a)->y) * ( (s)));                               \
    (o)->z = (((a)->z) * ( (s)));                               \
}                                                               \
MACRO_STOP

#define RwV3dDotProduct(a, b)                                   \
    ((((( (((a)->x) * ((b)->x))) +                              \
        ( (((a)->y) * ((b)->y))))) +                            \
        ( (((a)->z) * ((b)->z)))))

struct RwV3d
{
    float x;
    float y;
    float z;
};

struct RwMatrix
{
    RwV3d    right;
    uint32_t flags;
    RwV3d    up;
    uint32_t pad1;
    RwV3d    at;
    uint32_t pad2;
    RwV3d    pos;
    uint32_t pad3;
};

struct VertexTex2
{
    float      x;
    float      y;
    float      z;
    float      rhw;
    uint32_t   emissiveColor;
    float      u0;
    float      v0;
    float      u1;
    float      v1;
};

#define MAXSIZE 15
#define MINSIZE 4
#define DROPFVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2)
#define RAD2DEG(x) (180.0f*(x)/M_PI)

typedef HRESULT(STDMETHODCALLTYPE* CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
typedef HRESULT(STDMETHODCALLTYPE* Present_t)(LPDIRECT3DDEVICE9, CONST RECT *, CONST RECT *, HWND, CONST RGNDATA *);
typedef HRESULT(STDMETHODCALLTYPE* EndScene_t)(LPDIRECT3DDEVICE9);
typedef HRESULT(STDMETHODCALLTYPE* Reset_t)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);

class Interval
{
private:
    unsigned int initial_;

public:
    inline Interval() : initial_(GetTickCount()) { }

    virtual ~Interval() { }

    inline unsigned int value() const
    {
        return GetTickCount() - initial_;
    }
};

class Fps
{
protected:
    unsigned int m_fps;
    unsigned int m_fpscount;
    Interval m_fpsinterval;

public:
    Fps() : m_fps(0), m_fpscount(0) { }

    void update()
    {
        m_fpscount++;

        if (m_fpsinterval.value() > 1000)
        {
            m_fps = m_fpscount;
            m_fpscount = 0;
            m_fpsinterval = Interval();
        }
    }

    unsigned int get() const
    {
        return m_fps;
    }
};

class WaterDrop
{
public:
    float x, y, time;
    float size, uvsize, ttl;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t alpha;
    bool active;
    bool fades;
    void Fade();
};

class WaterDropMoving
{
public:
    WaterDrop* drop;
    float dist;
};

class WaterDrops
{
public:
    enum {
        MAXDROPS = 2000,
        MAXDROPSMOVING = 700
    };

    static inline Fps fps;
    static inline float* fTimeStep;
    static inline bool isPaused = false;
    static inline float ms_scaling;
#define SC(x) ((int32_t)((x)*ms_scaling))
    static inline float ms_xOff;
    static inline float ms_yOff;
    static inline WaterDrop ms_drops[MAXDROPS];
    static inline int32_t ms_numDrops;
    static inline WaterDropMoving ms_dropsMoving[MAXDROPSMOVING];
    static inline int32_t ms_numDropsMoving;

    static inline bool ms_enabled;
    static inline bool ms_movingEnabled;

    static inline float ms_distMoved;
    static inline float ms_vecLen;
    static inline float ms_rainStrength;
    static inline float ms_rainIntensity = 1.0f;
    static inline RwV3d ms_vec;
    static inline RwV3d ms_lastAt;
    static inline RwV3d ms_lastPos;
    static inline RwV3d ms_posDelta;

    static inline int32_t ms_splashDuration;
    static inline RwV3d   ms_splashPoint;
    static inline float   ms_splashDistance;

    static inline bool sprayWater = false;
    static inline bool sprayBlood = false;
    static inline bool ms_noCamTurns = false;
    static inline bool ms_StaticRain = false;

    static inline RwV3d right;
    static inline RwV3d up;
    static inline RwV3d at;
    static inline RwV3d pos;

    static inline uint32_t* pEndScene = nullptr;
    static inline uint32_t* pReset = nullptr;
    static inline CreateDevice_t RealD3D9CreateDevice = NULL;
    static inline Present_t RealD3D9Present = NULL;
    static inline EndScene_t RealD3D9EndScene = NULL;
    static inline Reset_t RealD3D9Reset = NULL;
    static inline bool bPatchD3D = true;

    static inline void(*WaterDrops::ProcessCallback1)();
    static inline void(*WaterDrops::ProcessCallback2)();

    static inline void Process(LPDIRECT3DDEVICE9 pDevice)
    {
        fps.update();
        if (!ms_initialised)
            InitialiseRender(pDevice);
        WaterDrops::CalculateMovement();
        WaterDrops::SprayDrops();
        WaterDrops::ProcessMoving();
        WaterDrops::Fade();
    }

    static inline void CalculateMovement()
    {
        RwV3dSub(&ms_posDelta, &pos, &ms_lastPos);

        ms_distMoved = RwV3dDotProduct(&ms_posDelta, &ms_posDelta);
        ms_distMoved = sqrt(ms_distMoved);
        //ms_distMoved = RwV3dLength(&ms_posDelta);

        ms_lastAt = at;
        ms_lastPos = pos;

        ms_vec.x = -RwV3dDotProduct(&right, &ms_posDelta);
        ms_vec.y = RwV3dDotProduct(&up, &ms_posDelta);
        ms_vec.z = RwV3dDotProduct(&at, &ms_posDelta);
        RwV3dScale(&ms_vec, &ms_vec, 10.0f);
        ms_vecLen = sqrt(ms_vec.y*ms_vec.y + ms_vec.x*ms_vec.x);

        ms_enabled = true; //!istopdown && !carlookdirection;
        ms_movingEnabled = true; //!istopdown && !carlookdirection;

        float c = at.z;
        if (c > 1.0f) c = 1.0f;
        if (c < -1.0f) c = -1.0f;
        ms_rainStrength = (float)RAD2DEG(acos(c));
    }

    static inline void SprayDrops()
    {
        static int32_t ndrops[] = {
            125, 250, 500, 1000, 1000,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };

        if (!NoRain() && ms_rainIntensity != 0.0f && ms_enabled) {
            auto tmp = (int32_t)(180.0f - ms_rainStrength);
            if (tmp < 40) tmp = 40;
            FillScreenMoving((tmp - 40.0f) / 150.0f * ms_rainIntensity * 0.5f);
        }
        if (sprayWater)
            FillScreenMoving(0.5f, false);
        if (sprayBlood)
            FillScreenMoving(0.5f, true);
        if (ms_splashDuration >= 0) {
            if (ms_numDrops < MAXDROPS) {
                RwV3d dist;
                RwV3dSub(&dist, &ms_splashPoint, &ms_lastPos);
                float f = RwV3dDotProduct(&dist, &dist);
                f = sqrt(f);
                if (f <= ms_splashDistance)
                    FillScreenMoving(1.0f);
            }
            ms_splashDuration--;
        }
    }

    static inline void MoveDrop(WaterDropMoving* moving)
    {
        WaterDrop *drop = moving->drop;
        if (!ms_movingEnabled)
            return;
        if (!drop->active) {
            moving->drop = NULL;
            ms_numDropsMoving--;
            return;
        }

        if (!ms_noCamTurns && (ms_vec.z <= 0.0f || ms_distMoved <= 0.3f))
            return;

        if (ms_vecLen <= 0.5f || ms_noCamTurns) {
            float d = abs(ms_vec.z*0.2f);
            float dx, dy, sum;
            dx = drop->x - ms_fbWidth * 0.5f + ms_vec.x;
            dy = drop->y - ms_fbHeight * 0.5f - ms_vec.y;
            sum = fabs(dx) + fabs(dy);
            if (sum >= 0.001f) {
                dx *= (1.0f / sum);
                dy *= (1.0f / sum);
            }
            moving->dist += d;
            if (moving->dist > 2.0f)
                NewTrace(moving);
            drop->x += dx * d;
            drop->y += dy * d;
        }
        else {
            // movement when camera turns
            moving->dist += ms_vecLen;
            if (moving->dist > 20.0f)
                NewTrace(moving);
            drop->x -= ms_vec.x;
            drop->y += ms_vec.y;
        }

        if (drop->x < 0.0f || drop->y < 0.0f ||
            drop->x >(ms_fbWidth + SC(MAXSIZE)) || drop->y >(ms_fbHeight + SC(MAXSIZE))) {
            moving->drop = NULL;
            ms_numDropsMoving--;
        }
    }

    static inline void ProcessMoving()
    {
        WaterDropMoving *moving;
        if (!ms_movingEnabled)
            return;
        for (moving = ms_dropsMoving; moving < &ms_dropsMoving[MAXDROPSMOVING]; moving++)
            if (moving->drop)
                MoveDrop(moving);
    }

    static inline void Fade()
    {
        WaterDrop *drop;
        for (drop = &ms_drops[0]; drop < &ms_drops[MAXDROPS]; drop++)
            if (drop->active)
                drop->Fade();
    }

    static inline WaterDrop* PlaceNew(float x, float y, float size, float ttl, bool fades, int R = 0xFF, int G = 0xFF, int B = 0xFF)
    {
        WaterDrop *drop;
        int i;

        if (NoDrops())
            return NULL;

        for (i = 0, drop = ms_drops; i < MAXDROPS; i++, drop++)
            if (ms_drops[i].active == 0)
                goto found;
        return NULL;
    found:
        ms_numDrops++;
        drop->x = x;
        drop->y = y;
        drop->size = size;
        drop->uvsize = (SC(MAXSIZE) - size + 1.0f) / (SC(MAXSIZE) - SC(MINSIZE) + 1.0f);
        drop->fades = fades;
        drop->active = 1;
        drop->r = R;
        drop->g = G;
        drop->b = B;
        drop->alpha = 0xFF;
        drop->time = 0.0f;
        drop->ttl = ttl;
        return drop;
    }

    static inline void NewTrace(WaterDropMoving* moving)
    {
        if (ms_numDrops < MAXDROPS) {
            moving->dist = 0.0f;
            PlaceNew(moving->drop->x, moving->drop->y, (float)(SC(MINSIZE)), 500.0f, 1, moving->drop->r, moving->drop->g, moving->drop->b);
        }
    }

    static inline void NewDropMoving(WaterDrop *drop)
    {
        WaterDropMoving *moving;
        for (moving = ms_dropsMoving; moving < &ms_dropsMoving[MAXDROPSMOVING]; moving++)
            if (moving->drop == NULL)
                goto found;
        return;
    found:
        ms_numDropsMoving++;
        moving->drop = drop;
        moving->dist = 0.0f;
    }

    static inline void FillScreenMoving(float amount, bool isBlood = false)
    {
        if (ms_StaticRain)
            amount = 1.0f;

        int32_t n = int32_t((ms_vec.z <= 5.0f ? 1.0f : 1.5f) * amount * 20.0f);
        WaterDrop *drop;

        while (n--)
            if (ms_numDrops < MAXDROPS && ms_numDropsMoving < MAXDROPSMOVING) {
                float x = (float)(rand() % ms_fbWidth);
                float y = (float)(rand() % ms_fbHeight);
                float time = (float)(rand() % (SC(MAXSIZE) - SC(MINSIZE)) + SC(MINSIZE));
                if (!isBlood)
                    drop = PlaceNew(x, y, time, 2000.0f, 1);
                else
                    drop = PlaceNew(x, y, time, 2000.0f, 1, 0xFF, 0x00, 0x00);
                if (drop)
                    NewDropMoving(drop);
            }
    }

    static inline void FillScreen(int n)
    {
        if (!ms_initialised)
            return;

        ms_numDrops = 0;
        for (auto drop = &ms_drops[0]; drop < &ms_drops[MAXDROPS]; drop++) {
            drop->active = 0;
            if (drop < &ms_drops[n]) {
                float x = (float)(rand() % ms_fbWidth);
                float y = (float)(rand() % ms_fbHeight);
                float time = (float)(rand() % (SC(MAXSIZE) - SC(MINSIZE)) + SC(MINSIZE));
                PlaceNew(x, y, time, 2000.0f, 1);
            }
        }
    }

    static inline void Clear()
    {
        for (auto drop = &ms_drops[0]; drop < &ms_drops[MAXDROPS]; drop++)
            drop->active = 0;
        ms_numDrops = 0;
    }

    static inline void Reset()
    {
        Clear();
        ms_splashDuration = -1;
        ms_splashDistance = 0;
        ms_splashPoint = { 0 };

        auto SafeRelease = [](auto ppT) {
            if (*ppT)
            {
                (*ppT)->Release();
                *ppT = NULL;
            }
        };

        SafeRelease(&WaterDrops::ms_maskTex);
        SafeRelease(&WaterDrops::ms_tex);
        SafeRelease(&WaterDrops::ms_surf);
        SafeRelease(&WaterDrops::ms_bbuf);
        WaterDrops::ms_initialised = 0;
    }

    static inline void RegisterSplash(RwV3d* point, float distance = 20.0f, int32_t duration = 14)
    {
        ms_splashPoint = *point;
        ms_splashDistance = distance;
        ms_splashDuration = duration;
    }

    static inline bool NoDrops()
    {
        return false; //CWeather__UnderWaterness > 0.339731634f || *CEntryExitManager__ms_exitEnterState != 0;
    }

    static inline bool NoRain()
    {
        return false; //CCullZones__CamNoRain() || CCullZones__PlayerNoRain() || *CGame__currArea != 0 || NoDrops();
    }

    // Rendering
    static inline IDirect3DTexture9* ms_maskTex;
    static inline IDirect3DTexture9* ms_tex;
    static inline IDirect3DSurface9* ms_surf;
    static inline IDirect3DSurface9* ms_bbuf;
    static inline int32_t ms_fbWidth;
    static inline int32_t ms_fbHeight;
    static inline IDirect3DVertexBuffer9* ms_vertexBuf;
    static inline IDirect3DIndexBuffer9 *ms_indexBuf;
    static inline VertexTex2* ms_vertPtr;
    static inline int32_t ms_numBatchedDrops;
    static inline int32_t ms_initialised;

    static inline void InitialiseRender(LPDIRECT3DDEVICE9 pDevice)
    {
        srand((uint32_t)time(NULL));

        IDirect3DVertexBuffer9 *vbuf;
        IDirect3DIndexBuffer9 *ibuf;
        pDevice->CreateVertexBuffer(MAXDROPS * 4 * sizeof(VertexTex2), D3DUSAGE_WRITEONLY, DROPFVF, D3DPOOL_MANAGED, &vbuf, nullptr);
        ms_vertexBuf = vbuf;
        pDevice->CreateIndexBuffer(MAXDROPS * 6 * sizeof(short), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ibuf, nullptr);
        ms_indexBuf = ibuf;
        uint16_t *idx;
        ibuf->Lock(0, 0, (void**)&idx, 0);
        for (int i = 0; i < MAXDROPS; i++) {
            idx[i * 6 + 0] = i * 4 + 0;
            idx[i * 6 + 1] = i * 4 + 1;
            idx[i * 6 + 2] = i * 4 + 2;
            idx[i * 6 + 3] = i * 4 + 0;
            idx[i * 6 + 4] = i * 4 + 2;
            idx[i * 6 + 5] = i * 4 + 3;
        }
        ibuf->Unlock();

        D3DSURFACE_DESC d3dsDesc;
        pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &ms_bbuf);
        ms_bbuf->GetDesc(&d3dsDesc);
        pDevice->CreateTexture(d3dsDesc.Width, d3dsDesc.Height, 1, D3DUSAGE_RENDERTARGET, d3dsDesc.Format, D3DPOOL_DEFAULT, &ms_tex, NULL);
        ms_tex->GetSurfaceLevel(0, &ms_surf);
        ms_fbWidth = d3dsDesc.Width;
        ms_fbHeight = d3dsDesc.Height;
        ms_scaling = ms_fbHeight / 480.0f;

        constexpr uint8_t dropmask_png[] = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
            0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x08, 0x06, 0x00, 0x00, 0x00, 0xAA, 0x69, 0x71,
            0xDE, 0x00, 0x00, 0x00, 0x13, 0x74, 0x45, 0x58, 0x74, 0x53, 0x6F, 0x66, 0x74, 0x77, 0x61, 0x72,
            0x65, 0x00, 0x52, 0x65, 0x6E, 0x64, 0x65, 0x72, 0x57, 0x61, 0x72, 0x65, 0xCE, 0xFE, 0x12, 0x23,
            0x00, 0x00, 0x01, 0x6C, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0xED, 0x9B, 0xD1, 0xAE, 0x84, 0x20,
            0x0C, 0x44, 0x5B, 0xFF, 0xFF, 0x9F, 0xBD, 0x4F, 0xBD, 0x21, 0xAC, 0x5A, 0x84, 0x4A, 0xA1, 0xD3,
            0xF3, 0xB6, 0x09, 0x22, 0x33, 0xED, 0xB8, 0x66, 0x17, 0x98, 0x1C, 0x38, 0xCF, 0xF3, 0xD4, 0xC6,
            0x30, 0x33, 0xCF, 0x58, 0xCB, 0xE7, 0x37, 0x69, 0x11, 0xAB, 0xF1, 0xA5, 0x19, 0xA6, 0x13, 0x5B,
            0x88, 0x6D, 0xC1, 0xD2, 0x10, 0xB3, 0x89, 0x66, 0x89, 0x17, 0xAC, 0x4C, 0x18, 0x9E, 0x64, 0xB6,
            0xF0, 0x9A, 0x51, 0x23, 0x86, 0x2E, 0xF6, 0x16, 0x2F, 0x8C, 0x98, 0xD0, 0x75, 0xE1, 0x2A, 0xC2,
            0x6B, 0x7A, 0x8C, 0x78, 0x7D, 0xC1, 0xAA, 0xE2, 0x85, 0xB7, 0x26, 0x34, 0x0F, 0x5E, 0x5D, 0x78,
            0x4D, 0xAB, 0x11, 0x4D, 0x83, 0x76, 0x13, 0x2F, 0xB4, 0x98, 0x70, 0x68, 0x03, 0x76, 0x15, 0xDF,
            0x8A, 0x6A, 0xC0, 0xCE, 0x34, 0xBD, 0x72, 0x8F, 0x4E, 0xB0, 0x03, 0x4F, 0x51, 0xB8, 0xED, 0x80,
            0x28, 0xE2, 0x35, 0x42, 0x47, 0x40, 0x78, 0x2A, 0xE6, 0x65, 0x6B, 0x44, 0xAD, 0xFE, 0x55, 0x14,
            0x7E, 0x3A, 0x20, 0xAA, 0xF8, 0x3B, 0x20, 0x22, 0x20, 0x5C, 0x15, 0x97, 0xB5, 0x01, 0x11, 0x29,
            0xA3, 0xF0, 0xDF, 0x01, 0x28, 0xE2, 0x6B, 0xA0, 0x22, 0x20, 0x94, 0xC5, 0x86, 0x34, 0xA0, 0x24,
            0x0D, 0xF0, 0x5E, 0x80, 0x37, 0x07, 0x11, 0xEE, 0x03, 0x90, 0x08, 0xB8, 0x03, 0xA4, 0xE8, 0xB0,
            0x06, 0x08, 0x69, 0x80, 0xF7, 0x02, 0xBC, 0x61, 0xE4, 0x07, 0x20, 0x51, 0x76, 0x40, 0x1A, 0x90,
            0x06, 0x78, 0x2F, 0xC0, 0x9B, 0x34, 0xC0, 0x7B, 0x01, 0xDE, 0xA4, 0x01, 0xDE, 0x0B, 0xF0, 0x84,
            0x99, 0xF9, 0x98, 0xB5, 0x1B, 0x6B, 0x55, 0xA0, 0x3B, 0x80, 0x28, 0x0D, 0xC0, 0x35, 0x40, 0xA2,
            0x7F, 0x94, 0x1F, 0x10, 0x81, 0xED, 0x00, 0x01, 0xD2, 0x80, 0xCB, 0x7F, 0x86, 0x50, 0x63, 0xF0,
            0x23, 0x3A, 0xFA, 0x0F, 0x24, 0x75, 0xA1, 0xA1, 0x22, 0xD0, 0xB4, 0x3F, 0x00, 0x2D, 0x0A, 0xB7,
            0x62, 0xA3, 0x45, 0xE1, 0xAE, 0xB0, 0x10, 0x11, 0xE8, 0xDA, 0x25, 0x86, 0x12, 0x05, 0x55, 0xE4,
            0xEE, 0x51, 0xD0, 0x0A, 0x19, 0x3A, 0x02, 0x26, 0x7B, 0x85, 0xA3, 0x47, 0xE1, 0x95, 0xB8, 0x5D,
            0xE2, 0xF0, 0xA6, 0x68, 0x79, 0x60, 0xA2, 0xF7, 0x46, 0xAB, 0x19, 0xD1, 0x1B, 0xD5, 0x3C, 0x34,
            0x65, 0xB1, 0x00, 0x2F, 0x23, 0x2C, 0x1E, 0xD0, 0x79, 0x70, 0xD2, 0x62, 0x92, 0x92, 0xAF, 0x8D,
            0xB0, 0xFE, 0x5A, 0xCE, 0xC3, 0xD3, 0x5F, 0x4D, 0xFC, 0x84, 0x66, 0xCA, 0xCC, 0x97, 0xAF, 0x3F,
            0x12, 0xC8, 0x90, 0x6B, 0x76, 0x10, 0xAE, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44,
            0xAE, 0x42, 0x60, 0x82
        };

        D3DXCreateTextureFromFileInMemory(pDevice, &dropmask_png[0], sizeof(dropmask_png), &ms_maskTex);

        ms_initialised = 1;
    }

    static inline void AddToRenderList(WaterDrop *drop)
    {
        static float xy[] = {
            -1.0f, -1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f, -1.0f
        };
        static float uv[] = {
            0.0f, 0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 0.0f
        };
        int i;
        float scale;

        float u1_1, u1_2;
        float v1_1, v1_2;
        float tmp;

        tmp = drop->uvsize*(300.0f - 40.0f) + 40.0f;
        u1_1 = drop->x + ms_xOff - tmp;
        v1_1 = drop->y + ms_yOff - tmp;
        u1_2 = drop->x + ms_xOff + tmp;
        v1_2 = drop->y + ms_yOff + tmp;
        u1_1 = (u1_1 <= 0.0f ? 0.0f : u1_1) / ms_fbWidth;
        v1_1 = (v1_1 <= 0.0f ? 0.0f : v1_1) / ms_fbHeight;
        u1_2 = (u1_2 >= ms_fbWidth ? ms_fbWidth : u1_2) / ms_fbWidth;
        v1_2 = (v1_2 >= ms_fbHeight ? ms_fbHeight : v1_2) / ms_fbHeight;

        scale = drop->size * 0.5f;

        for (i = 0; i < 4; i++) {
            ms_vertPtr->x = drop->x + xy[i * 2] * scale + ms_xOff;
            ms_vertPtr->y = drop->y + xy[i * 2 + 1] * scale + ms_yOff;
            ms_vertPtr->z = 0.0f;
            ms_vertPtr->rhw = 1.0f;
            ms_vertPtr->emissiveColor = D3DCOLOR_ARGB(drop->alpha, drop->r, drop->g, drop->b);
            ms_vertPtr->u0 = uv[i * 2];
            ms_vertPtr->v0 = uv[i * 2 + 1];
            ms_vertPtr->u1 = i >= 2 ? u1_2 : u1_1;
            ms_vertPtr->v1 = i % 3 == 0 ? v1_2 : v1_1;
            ms_vertPtr++;
        }
        ms_numBatchedDrops++;
    }

    static inline void Render(LPDIRECT3DDEVICE9 pDevice)
    {
        if (!ms_enabled || ms_numDrops <= 0)
            return;

        IDirect3DVertexBuffer9 *vbuf = ms_vertexBuf;
        vbuf->Lock(0, 0, (void**)&ms_vertPtr, 0);
        ms_numBatchedDrops = 0;
        for (WaterDrop *drop = &ms_drops[0]; drop < &ms_drops[MAXDROPS]; drop++)
            if (drop->active)
                AddToRenderList(drop);
        vbuf->Unlock();

        LPDIRECT3DSTATEBLOCK9 pStateBlock = NULL;
        pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock);
        pStateBlock->Capture();

        pDevice->StretchRect(ms_bbuf, NULL, ms_surf, NULL, D3DTEXF_LINEAR);

        pDevice->SetTexture(0, ms_maskTex);
        pDevice->SetTexture(1, ms_tex);

        pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
        pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
        pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
        pDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        pDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
        pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        pDevice->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        pDevice->SetRenderState(D3DRS_LIGHTING, 0);
        pDevice->SetRenderState(D3DRS_ZENABLE, 0);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
        pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, 0);
        pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, 1);
        pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);

        pDevice->SetFVF(DROPFVF);
        pDevice->SetStreamSource(0, vbuf, 0, sizeof(VertexTex2));
        pDevice->SetIndices(ms_indexBuf);
        pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, ms_numBatchedDrops * 4, 0, ms_numBatchedDrops * 2);

        pDevice->SetTexture(1, NULL);
        pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

        pStateBlock->Apply();
        pStateBlock->Release();
    }

    static inline HRESULT WINAPI d3dReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
    {
        WaterDrops::Reset();
        return RealD3D9Reset(pDevice, pPresentationParameters);
    }

    static inline HRESULT WINAPI d3dEndScene(LPDIRECT3DDEVICE9 pDevice)
    {
        if (WaterDrops::ProcessCallback1)
            WaterDrops::ProcessCallback1();
        WaterDrops::Process(pDevice);
        WaterDrops::Render(pDevice);
        if (WaterDrops::ProcessCallback2)
            WaterDrops::ProcessCallback2();
        return RealD3D9EndScene(pDevice);
    }

    static inline HRESULT WINAPI d3d8EndScene(LPDIRECT3DDEVICE9 pDevice)
    {
        class Direct3DDevice8 : public IUnknown
        {
            //...
        public:
            void* ProxyAddressLookupTable;
            void* const D3D;
            IDirect3DDevice9 *const ProxyInterface;
            //...
        };

        if (WaterDrops::ProcessCallback1)
            WaterDrops::ProcessCallback1();
        WaterDrops::Process(((Direct3DDevice8*)pDevice)->ProxyInterface);
        WaterDrops::Render(((Direct3DDevice8*)pDevice)->ProxyInterface);
        if (WaterDrops::ProcessCallback2)
            WaterDrops::ProcessCallback2();
        return RealD3D9EndScene(pDevice);
    }

    static inline HRESULT WINAPI d3dCreateDevice(IDirect3D9* d3ddev, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
    {
        HRESULT retval = RealD3D9CreateDevice(d3ddev, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
        UINT_PTR* pVTable = (UINT_PTR*)(*((UINT_PTR*)*ppReturnedDeviceInterface));

        if (!RealD3D9EndScene)
            RealD3D9EndScene = (EndScene_t)pVTable[IDirect3DDevice9VTBL::EndScene];
        pEndScene = &pVTable[IDirect3DDevice9VTBL::EndScene];

        if (!RealD3D9Reset)
            RealD3D9Reset = (Reset_t)pVTable[IDirect3DDevice9VTBL::Reset];
        pReset = &pVTable[IDirect3DDevice9VTBL::Reset];

        if (bPatchD3D)
        {
            injector::WriteMemory(pEndScene, &d3dEndScene, true);
            injector::WriteMemory(pReset, &d3dReset, true);
        }
        return retval;
    }

    static inline HRESULT WINAPI d3d8CreateDevice(IDirect3D9* d3ddev, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
    {
        HRESULT retval = RealD3D9CreateDevice(d3ddev, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
        UINT_PTR* pVTable = (UINT_PTR*)(*((UINT_PTR*)*ppReturnedDeviceInterface));

        if (!RealD3D9EndScene)
            RealD3D9EndScene = (EndScene_t)pVTable[IDirect3DDevice8VTBL::EndScene];
        pEndScene = &pVTable[IDirect3DDevice8VTBL::EndScene];

        if (!RealD3D9Reset)
            RealD3D9Reset = (Reset_t)pVTable[IDirect3DDevice8VTBL::Reset];
        pReset = &pVTable[IDirect3DDevice8VTBL::Reset];

        if (bPatchD3D)
        {
            injector::WriteMemory(pEndScene, &d3d8EndScene, true);
            injector::WriteMemory(pReset, &d3dReset, true);
        }
        return retval;
    }
};

void WaterDrop::Fade()
{
    float delta = 0.0f;
    if (!WaterDrops::fTimeStep)
    {
        auto dt = ((1.0f / WaterDrops::fps.get()) / 2.0f) * 100.0f;
        delta = (float)(((dt > 3.0f) ? 3.0f : ((dt < 0.0000099999997f) ? 0.0000099999997f : dt)) * 1000.0f / 50.0f);
        if (WaterDrops::isPaused)
            delta = 0.0f;
    }
    else
        delta = *WaterDrops::fTimeStep * 1000.0f;
    this->time += delta;
    if (this->time >= this->ttl) {
        WaterDrops::ms_numDrops--;
        this->active = 0;
    }
    else if (this->fades)
        this->alpha = (int8_t)(255.0f - time / ttl * 255.0f);
}

class CallbackHandler
{
public:
    static inline void CreateThreadAutoClose(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
    {
        CloseHandle(CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId));
    }

    static inline void RegisterCallback(std::function<void()>&& fn)
    {
        fn();
    }

    static inline void RegisterCallback(std::wstring_view module_name, std::function<void()>&& fn)
    {
        if (module_name.empty() || GetModuleHandleW(module_name.data()) != NULL)
        {
            fn();
        }
        else
        {
            RegisterDllNotification();
            GetCallbackList().emplace(module_name, std::forward<std::function<void()>>(fn));
        }
    }

    static inline void RegisterCallback(std::function<void()>&& fn, bool bPatternNotFound, ptrdiff_t offset = 0x1100, uint32_t* ptr = nullptr)
    {
        if (!bPatternNotFound)
        {
            fn();
        }
        else
        {
            auto mh = GetModuleHandle(NULL);
            IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((DWORD)mh + ((IMAGE_DOS_HEADER*)mh)->e_lfanew);
            if (ptr == nullptr)
                ptr = (uint32_t*)((DWORD)mh + ntHeader->OptionalHeader.BaseOfCode + ntHeader->OptionalHeader.SizeOfCode - offset);
            std::thread([](std::function<void()>&& fn, uint32_t* ptr, uint32_t val)
                {
                    while (*ptr == val)
                        std::this_thread::yield();

                    fn();
                }, fn, ptr, *ptr).detach();
        }
    }

    static inline void RegisterCallback(std::function<void()>&& fn, hook::pattern pattern)
    {
        if (!pattern.empty())
        {
            fn();
        }
        else
        {
            auto* ptr = new ThreadParams{ fn, pattern };
            CreateThreadAutoClose(0, 0, (LPTHREAD_START_ROUTINE)&ThreadProc, (LPVOID)ptr, 0, NULL);
        }
    }

private:
    static inline void call(std::wstring_view module_name)
    {
        if (GetCallbackList().count(module_name.data()))
        {
            GetCallbackList().at(module_name.data())();
            GetCallbackList().erase(module_name.data());
        }

        //if (GetCallbackList().empty()) //win7 crash in splinter cell
        //    UnRegisterDllNotification();
    }

    static inline void invoke_all()
    {
        for (auto&& fn : GetCallbackList())
            fn.second();
    }

private:
    struct Comparator {
        bool operator() (const std::wstring& s1, const std::wstring& s2) const {
            std::wstring str1(s1.length(), ' ');
            std::wstring str2(s2.length(), ' ');
            std::transform(s1.begin(), s1.end(), str1.begin(), tolower);
            std::transform(s2.begin(), s2.end(), str2.begin(), tolower);
            return  str1 < str2;
        }
    };

    static std::map<std::wstring, std::function<void()>, Comparator>& GetCallbackList()
    {
        static std::map<std::wstring, std::function<void()>, Comparator> functions;
        return functions;
    }

    struct ThreadParams
    {
        std::function<void()> fn;
        hook::pattern pattern;
    };

    typedef NTSTATUS(NTAPI* _LdrRegisterDllNotification) (ULONG, PVOID, PVOID, PVOID);
    typedef NTSTATUS(NTAPI* _LdrUnregisterDllNotification) (PVOID);

    typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
        ULONG Flags;                    //Reserved.
        PUNICODE_STRING FullDllName;    //The full path name of the DLL module.
        PUNICODE_STRING BaseDllName;    //The base file name of the DLL module.
        PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
        ULONG SizeOfImage;              //The size of the DLL image, in bytes.
    } LDR_DLL_LOADED_NOTIFICATION_DATA, LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

    typedef union _LDR_DLL_NOTIFICATION_DATA {
        LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
        LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
    } LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

private:
    static inline void CALLBACK LdrDllNotification(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
    {
        static constexpr auto LDR_DLL_NOTIFICATION_REASON_LOADED = 1;
        if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED)
        {
            call(NotificationData->Loaded.BaseDllName->Buffer);
        }
    }

    static inline void RegisterDllNotification()
    {
        LdrRegisterDllNotification = (_LdrRegisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification");
        if (LdrRegisterDllNotification && !cookie)
            LdrRegisterDllNotification(0, LdrDllNotification, 0, &cookie);
    }

    static inline void UnRegisterDllNotification()
    {
        LdrUnregisterDllNotification = (_LdrUnregisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrUnregisterDllNotification");
        if (LdrUnregisterDllNotification && cookie)
            LdrUnregisterDllNotification(cookie);
    }

    static inline DWORD WINAPI ThreadProc(LPVOID ptr)
    {
        auto paramsPtr = static_cast<CallbackHandler::ThreadParams*>(ptr);
        auto params = *paramsPtr;
        delete paramsPtr;

        HANDLE hTimer = NULL;
        LARGE_INTEGER liDueTime;
        liDueTime.QuadPart = -30 * 10000000LL;
        hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
        SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0);

        while (params.pattern.clear().empty())
        {
            Sleep(0);

            if (WaitForSingleObject(hTimer, 0) == WAIT_OBJECT_0)
            {
                CloseHandle(hTimer);
                return 0;
            }
        };

        params.fn();

        return 0;
    }
private:
    static inline _LdrRegisterDllNotification   LdrRegisterDllNotification;
    static inline _LdrUnregisterDllNotification LdrUnregisterDllNotification;
    static inline void* cookie;
public:
    static inline std::once_flag flag;
};