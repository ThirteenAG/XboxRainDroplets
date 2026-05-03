#include "xrd.h"

namespace rage
{
    class Vector3
    {
    public:
        float x, y, z;

    public:
        Vector3() = default;
        Vector3(float x, float y, float z) : x(x), y(y), z(z)
        {
        }

        Vector3 operator+(const Vector3& other) const
        {
            return Vector3(x + other.x, y + other.y, z + other.z);
        }

        Vector3 operator-(const Vector3& other) const
        {
            return Vector3(x - other.x, y - other.y, z - other.z);
        }

        Vector3 operator*(float scalar) const
        {
            return Vector3(x * scalar, y * scalar, z * scalar);
        }

        Vector3 operator/(float scalar) const
        {
            if (scalar != 0.0f)
                return Vector3(x / scalar, y / scalar, z / scalar);
            else
                throw std::runtime_error("Division by zero");
        }

        Vector3& operator=(const Vector3& other)
        {
            if (this != &other)
            {
                x = other.x;
                y = other.y;
                z = other.z;
            }
            return *this;
        }

        Vector3& operator+=(const Vector3& other)
        {
            x += other.x;
            y += other.y;
            z += other.z;
            return *this;
        }

        Vector3& operator+=(float scalar)
        {
            x += scalar;
            y += scalar;
            z += scalar;
            return *this;
        }

        Vector3& operator-=(const Vector3& other)
        {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            return *this;
        }

        Vector3& operator*=(const Vector3& other)
        {
            x *= other.x;
            y *= other.y;
            z *= other.z;
            return *this;
        }

        bool operator==(const Vector3& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }

        bool operator!=(const Vector3& other) const
        {
            return !(*this == other);
        }

        float Heading() const
        {
            return atan2f(-x, y);
        }

        float Magnitude()
        {
            return sqrt(x * x + y * y + z * z);
        }

        void Translate(float x, float y, float z)
        {
            this->x += x;
            this->y += y;
            this->z += z;
        }

        void Normalize()
        {
            float mag = Magnitude();
            if (mag > 0.0f)
            {
                x /= mag;
                y /= mag;
                z /= mag;
            }
            else
            {
                x = 0.0f;
                y = 0.0f;
                z = 0.0f;
            }
        }
    };

    class Vector4
    {
    public:
        float x, y, z, w;

    public:
        Vector4() = default;
        Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w)
        {
        }
        Vector4(const Vector3& other) : x(other.x), y(other.y), z(other.z), w(0.0f)
        {
        }

        operator Vector3() const
        {
            return Vector3(x, y, z);
        }

        Vector4 operator+(const Vector4& other) const
        {
            return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
        }

        Vector4 operator-(const Vector4& other) const
        {
            return Vector4(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        Vector4 operator*(float scalar) const
        {
            return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
        }

        Vector4 operator+(float scalar) const
        {
            return Vector4(x + scalar, y + scalar, z + scalar, w + scalar);
        }

        Vector4 operator-(float scalar) const
        {
            return Vector4(x - scalar, y - scalar, z - scalar, w - scalar);
        }

        friend Vector4 operator*(float scalar, const Vector4& vec)
        {
            Vector4 result;

            result.x = vec.x * scalar;
            result.y = vec.y * scalar;
            result.z = vec.z * scalar;
            result.w = vec.w * scalar;

            return result;
        }

        Vector4 operator/(float scalar) const
        {
            if (scalar != 0.0f)
                return Vector4(x / scalar, y / scalar, z / scalar, w / scalar);
            else
                throw std::runtime_error("Division by zero");
        }

        Vector4& operator=(const Vector4& other)
        {
            if (this != &other)
            {
                x = other.x;
                y = other.y;
                z = other.z;
                w = other.w;
            }
            return *this;
        }

        Vector4& operator+=(const Vector4& other)
        {
            x += other.x;
            y += other.y;
            z += other.z;
            w += other.w;
            return *this;
        }

        Vector4& operator-=(const Vector4& other)
        {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            w -= other.w;
            return *this;
        }

        bool operator==(const Vector4& other) const
        {
            return x == other.x && y == other.y && z == other.z && w == other.w;
        }

        bool operator!=(const Vector4& other) const
        {
            return !(*this == other);
        }

        float Heading() const
        {
            return atan2f(-x, y);
        }

        float Magnitude()
        {
            return sqrt(x * x + y * y + z * z + w * w);
        }

        void Translate(float x, float y, float z, float w)
        {
            this->x += x;
            this->y += y;
            this->z += z;
            this->w += w;
        }

        void Translate(float x, float y)
        {
            this->x += x;
            this->y += y;
            this->z += x;
            this->w += y;
        }
    };

    Vector3 operator+(const Vector3& v3, const Vector4& v4)
    {
        return Vector3(v3.x + v4.x, v3.y + v4.y, v3.z + v4.z);
    }

    Vector3 operator+(const Vector4& v4, const Vector3& v3)
    {
        return Vector3(v4.x + v3.x, v4.y + v3.y, v4.z + v3.z);
    }

    class Matrix44
    {
    public:
        Vector4 right;
        Vector4 up;
        Vector4 at;
        Vector4 pos;
    };

    enum grcTextureFormat : uint8_t
    {
        GRCFMT_UNKNOWN,
        GRCFMT_R5G6B5,
        GRCFMT_A8R8G8B8,
        GRCFMT_R16F,
        GRCFMT_R32F,
        GRCFMT_A2R10G10B10,
        GRCFMT_A16B16G16R16F,
        GRCFMT_G16R16,
        GRCFMT_G16R16F,
        GRCFMT_A32B32G32R32F,
        GRCFMT_A16B16G16R16F2,
        GRCFMT_A16B16G16R16,
        GRCFMT_L8,
        GRCFMT_D24S8 = 0xE,
        GRCFMT_X8R8G8B8 = 0x10,
    };

    inline std::vector<std::pair<grcTextureFormat, D3DFORMAT>> m_Formats
    {
        { GRCFMT_UNKNOWN,        D3DFMT_UNKNOWN },
        { GRCFMT_R5G6B5,         D3DFMT_R5G6B5 },
        { GRCFMT_A8R8G8B8,       D3DFMT_A8R8G8B8 },
        { GRCFMT_R16F,           D3DFMT_R16F },
        { GRCFMT_R32F,           D3DFMT_R32F },
        { GRCFMT_A2R10G10B10,    D3DFMT_A2R10G10B10 },
        { GRCFMT_A16B16G16R16F,  D3DFMT_A16B16G16R16F },
        { GRCFMT_G16R16,         D3DFMT_G16R16 },
        { GRCFMT_G16R16F,        D3DFMT_G16R16F },
        { GRCFMT_A32B32G32R32F,  D3DFMT_A32B32G32R32F },
        { GRCFMT_A16B16G16R16F2, D3DFMT_A16B16G16R16F },
        { GRCFMT_A16B16G16R16,   D3DFMT_A16B16G16R16 },
        { GRCFMT_L8,             D3DFMT_L8 },
        { GRCFMT_D24S8,          D3DFMT_D24S8 },
        { GRCFMT_X8R8G8B8,       D3DFMT_X8R8G8B8 },
    };

    inline grcTextureFormat getEngineTextureFormat(D3DFORMAT format)
    {
        for (auto& pair : m_Formats)
        {
            if (pair.second == format)
                return pair.first;
        }
        return GRCFMT_UNKNOWN;
    }

    inline D3DFORMAT getD3DTextureFormat(grcTextureFormat format)
    {
        for (auto& pair : m_Formats)
        {
            if (pair.first == format)
                return pair.second;
        }
        return D3DFMT_UNKNOWN;
    }

    struct grcViewportWindow
    {
        float X;
        float Y;
        float Width;
        float Height;
        float MinZ;
        float MaxZ;
    };

    class grcViewport
    {
    public:
        float mWorldMatrix44[4][4];
        float mCameraMatrix[4][4];
        float mWorldMatrix[4][4];
        float mWorldViewMatrix[4][4];
        float mWorldViewProjMatrix[4][4];
        float mViewInverseMatrix[4][4];
        float mViewMatrix[4][4];
        float mProjectionMatrix[4][4];
        float mMatrix_200[4][4];//FrustumLRTB?
        float mMatrix_240[4][4];//FrustumNFNF?
        grcViewportWindow mGrcViewportWindow1;
        grcViewportWindow mGrcViewportWindow2;//UnclippedWindow?
        int mWidth;
        int mHeight;
        float mFov;
        float mAspect;
        float mNearClip;
        float mFarClip;
        float field_2C8;
        float field_2CC;
        float mScaleX;
        float mScaleY;
        float field_2D8;
        float field_2DC;
        Vector4 field_2E0;
        bool mIsPerspective;
        char gap_2f1[3];
        int field_2F4;
        int field_2F8;
        int field_2FC;
        Vector4 mFrustumClipPlanes[6];
        int field_360;
        int field_364;
        int field_368;
        int field_36C;
        int field_370;
        int field_374;
        int field_378;
        int field_37C;
        int field_380;
        int field_384;
        int field_388;
        int field_38C;
        int field_390;
        int field_394;
        int field_398;
        int field_39C;
        int field_3A0;
        int field_3A4;
        int field_3A8;
        int field_3AC;
        int field_3B0;
        int field_3B4;
        int field_3B8;
        int field_3BC;
        int field_3C0;
        int field_3C4;
        int field_3C8;
        int field_3CC;
        int field_3D0;
        int field_3D4;
        int field_3D8;
        int field_3DC;
        bool mInvertZInProjectionMatrix;
        char field_3E1[3];
        int field_3E4;
        int field_3E8;
        int field_3EC;
    };

    grcViewport** pCurrentViewport = nullptr;
    grcViewport* GetCurrentViewport()
    {
        return *pCurrentViewport;
    }

    struct grcImage
    {
        uint16_t mWidth;
        uint16_t mHeight;
        int32_t mFormat;
        int32_t mType;
        uint16_t mStride;
        uint16_t mDepth;
        void* mPixelData;
        int32_t* field_14;
        grcImage* mNextLevel;
        grcImage* mNextSlice;
        uint32_t mRefCount;
        uint8_t gap24[8];
        int field_2C;
        float field_30[3];
        int32_t field_3C;
        float field_40[3];
        int field_4C;
    };

    class datBase
    {
    protected:
        uint32_t* _vft;
    };

    class pgBase : public datBase
    {
        uint32_t mBlockMap;
    };

    class grcRenderTargetPC;
    struct grcRenderTargetDesc
    {
        grcRenderTargetDesc()
            : field_0(0)
            , mMultisampleCount(0)
            , field_8(1)
            , mLevels(1)
            , field_10(1)
            , field_11(1)
            , field_1C(1)
            , field_24(1)
            , field_26(1)
            , field_28(1)
            , field_12(0)
            , mDepthRT(0)
            , field_18(0)
            , field_20(0)
            , field_25(0)
            , field_27(0)
            , field_29(0)
            , field_2A(0)
            , mFormat(GRCFMT_UNKNOWN)
        {
        }

        char field_0;
        int mMultisampleCount;
        char field_8;
        int mLevels;
        char field_10;
        char field_11;
        char field_12;
        grcRenderTargetPC* mDepthRT;
        char field_18;
        int field_1C;
        int field_20;
        bool field_24;
        char field_25;
        char field_26;
        char field_27;
        char field_28;
        char field_29;
        char field_2A;
        alignas(4) grcTextureFormat mFormat;
    };

    class grcRenderTarget : public pgBase
    {
    public:
        uint8_t field_8;
        uint8_t field_9;
        int16_t field_A;
        int32_t field_C;
        int32_t field_10;
        uint32_t mType;
        int32_t* field_18;
        int16_t field_1C;
        int16_t field_1E;
        int32_t field_20;
        int16_t field_24;
        int16_t field_26;
        int32_t field_28;
        uint16_t field_2C;
        uint16_t field_2E;
    };

    class grcRenderTargetPC : public  grcRenderTarget
    {
    public:
        char* mName;
        IDirect3DTexture9* mD3DTexture;
        IDirect3DSurface9* mD3DSurface;
        uint16_t mWidth;
        uint16_t mHeight;
        grcTextureFormat mFormat;
        uint8_t mIndex;
        uint8_t mBitsPerPixel;
        uint8_t mMultisampleCount;
        bool field_44;
        uint8_t gap45[2];
        uint8_t field_47;
        uint32_t mLevels;
        bool field_4C;
        bool field_4D;
        uint8_t field_4E;

        void Destroy(uint8_t a2 = 1)
        {
            auto func = (void(__thiscall*)(grcRenderTargetPC*, uint8_t))(_vft[0]);
            func(this, a2);
        }
    };

    class grcTexturePC : pgBase
    {
    public:
        uint8_t field_8;
        uint8_t mDepth;
        uint16_t mRefCount;
        int32_t field_C;
        int32_t field_10;
        char* mName;
        IDirect3DTexture9* mD3DTexture;
        uint16_t mWidth;
        uint16_t mHeight;
        int32_t mFormat;
        uint16_t mStride;
        uint8_t mTextureType;
        uint8_t mMipCount;
        D3DVECTOR field_28;
        D3DVECTOR field_34;
        grcTexturePC* mPrevious;
        grcTexturePC* mNext;
        void* mPixelData;
        uint8_t field_4C;
        uint8_t field_4D;
        uint8_t field_4E;
        uint8_t field_4F;

        bool Init()
        {
            auto func = (grcTexturePC * (__thiscall*)(grcTexturePC*))(_vft[4]);
            return func(this);
        }
    };

    struct grcTextureReferenceBase : public pgBase
    {
        char field_8;
        char field_9;
        int16_t m_wUsageCount;
        int field_C;
    };

    struct grcTextureReference : public grcTextureReferenceBase
    {
        int field_10;
        const char* m_pszName;
        grcTexturePC* m_pTexture;
    };

    namespace grcDevice
    {
        struct grcResolveFlags
        {
            grcResolveFlags()
                : Depth(1.0f)
                , BlurKernelSize(1.0f)
                , Color(0)
                , Stencil(0)
                , ColorExpBias(0)
                , ClearColor(false)
                , ClearDepthStencil(false)
                , BlurResult(false)
                , NeedResolve(true)
                , MipMap(true)
            {
            }

            float Depth;
            float BlurKernelSize;
            uint32_t Color;
            uint32_t Stencil;
            int ColorExpBias;
            bool ClearColor;
            bool ClearDepthStencil;
            bool BlurResult;
            bool NeedResolve;
            bool MipMap;
        };

        IDirect3DDevice9** ms_pD3DDevice = nullptr;

        IDirect3DDevice9* GetD3DDevice()
        {
            return *ms_pD3DDevice;
        }

        void* SetCallbackAddr;
        class FunctorBase
        {
        public:
            FunctorBase()
            {
                memset(mMemFunc, 0xAA, 8);
                mCallee = 0;
            }

            FunctorBase(void* callee, void(__fastcall* function)(), void* mf, uint32_t size)
            {
                auto SetCallback = (void(__thiscall*)(FunctorBase*, void*, void(__fastcall*)(), void*, uint32_t))SetCallbackAddr;
                SetCallback(this, callee, function, mf, size);
            }

            union
            {
                void(__fastcall* mFunction)();
                uint8_t mMemFunc[8];
            };

            uint32_t mCallee;
        };

        class Functor0 : public FunctorBase
        {
        public:
            Functor0(void* callee, void(__fastcall* function)(), void* mf, uint32_t size)
                : FunctorBase(callee, function, mf, size)
            {
                mThunk = Translator;
            }

        private:
            void(__cdecl* mThunk)(FunctorBase*);

            static void Translator(FunctorBase* functor)
            {
                functor->mFunction();
            }
        };

        void(__cdecl* RegisterDeviceCallbacks)(Functor0 onLost, Functor0 onReset);
    }

    class grcTextureFactoryPC;
    class grcTextureFactory
    {
    protected:
        uint32_t* _vft;

    public:
        bool field_4;
        char _gap5[3];

        static inline grcTextureFactoryPC** g_pTextureFactory = nullptr;
        static grcTextureFactoryPC* GetInstance()
        {
            return *g_pTextureFactory;
        }
    };

    class grcTextureFactoryPC : public grcTextureFactory
    {
    public:
        IDirect3DSurface9* mPrevRenderTargets[3];
        int32_t field_14;
        int32_t field_18;
        int32_t field_1C;
        int32_t field_20;
        int32_t field_24;
        IDirect3DSurface9* mD3D9Surfaces[4];
        uint8_t gap38[4];
        int32_t field_3C;
        int32_t field_40;
        int32_t field_44;
        IDirect3DSurface9* field_48;
        IDirect3DSurface9* mDepthStencilSurface;
        IDirect3DSurface9* field_50;
        grcRenderTargetPC* field_54;
        int32_t field_58;
        int32_t field_5C;
        int32_t field_60;
        int32_t field_64;
        int32_t field_68;
        int32_t field_6C;
        int32_t field_70;

        //virtuals
        grcTexturePC* Create(grcImage* image, void* arg2)
        {
            auto func = (grcTexturePC * (__stdcall*)(grcImage*, void*))(_vft[2]);
            return func(image, arg2);
        }

        grcRenderTargetPC* __stdcall CreateRenderTarget(const char* name, int32_t a2, uint32_t width, uint32_t height, uint32_t bitsPerPixel, grcRenderTargetDesc* desc)
        {
            auto func = (grcRenderTargetPC * (__stdcall*)(const char*, int32_t, uint32_t, uint32_t, uint32_t, grcRenderTargetDesc*))(_vft[14]);
            return func(name, a2, width, height, bitsPerPixel, desc);
        }

        void LockRenderTarget(uint32_t index, grcRenderTargetPC* color, grcRenderTargetPC* depth, uint32_t a5 = 0, bool a6 = 1, uint32_t mip = 0)
        {
            auto func = (void(__thiscall*)(grcTextureFactory*, uint32_t, grcRenderTargetPC*, grcRenderTargetPC*, uint32_t, bool, uint32_t))(_vft[15]);
            func(this, index, color, depth, a5, a6, mip);
        }

        void UnlockRenderTarget(uint32_t index, grcDevice::grcResolveFlags* resolveFlags, int32_t unused = -1)
        {
            auto func = (void(__thiscall*)(grcTextureFactory*, uint32_t, grcDevice::grcResolveFlags*, int32_t))(_vft[16]);
            func(this, index, resolveFlags, unused);
        }
    };
}

rage::grcRenderTargetPC* ms_rainRT = nullptr;

static inline void WaterDrops__InitialiseRender(LPDIRECT3DDEVICE pDevice)
{
    WaterDrops::ms_drops.resize(WaterDrops::MaxDrops);
    WaterDrops::ms_dropsMoving.resize(WaterDrops::MaxDropsMoving);

    IDirect3DVertexBuffer* vbuf;
    IDirect3DIndexBuffer* ibuf;

    if (FAILED(pDevice->CreateVertexBuffer(WaterDrops::ms_drops.capacity() * 4 * sizeof(VertexTex2), D3DUSAGE_WRITEONLY, DROPFVF, D3DPOOL_MANAGED, &vbuf, nullptr)))
        pDevice->CreateVertexBuffer(WaterDrops::ms_drops.capacity() * 4 * sizeof(VertexTex2), D3DUSAGE_DYNAMIC, DROPFVF, D3DPOOL_DEFAULT, &vbuf, nullptr);

    if (FAILED(pDevice->CreateIndexBuffer(WaterDrops::ms_drops.capacity() * 6 * sizeof(short), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ibuf, nullptr)))
        pDevice->CreateIndexBuffer(WaterDrops::ms_drops.capacity() * 6 * sizeof(short), D3DUSAGE_DYNAMIC, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ibuf, nullptr);

    WaterDrops::ms_vertexBuf = vbuf;
    WaterDrops::ms_indexBuf = ibuf;

    uint16_t* idx;
    ibuf->Lock(0, 0, (void**)&idx, 0);
    for (auto i = 0; i < int32_t(WaterDrops::ms_drops.capacity()); i++)
    {
        idx[i * 6 + 0] = i * 4 + 0;
        idx[i * 6 + 1] = i * 4 + 1;
        idx[i * 6 + 2] = i * 4 + 2;
        idx[i * 6 + 3] = i * 4 + 0;
        idx[i * 6 + 4] = i * 4 + 2;
        idx[i * 6 + 5] = i * 4 + 3;
    }
    ibuf->Unlock();

    D3DSURFACE_DESC d3dsDesc;
    pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &WaterDrops::ms_bbuf);
    WaterDrops::ms_bbuf->GetDesc(&d3dsDesc);

    rage::grcRenderTargetDesc desc{};
    desc.mMultisampleCount = 0;
    desc.field_0 = 1;
    desc.field_12 = 1;
    desc.mDepthRT = nullptr;
    desc.field_8 = 1;
    desc.field_10 = 1;
    desc.field_11 = 1;
    desc.field_24 = false;

    auto CreateEmptyRT = [](const char* name, int32_t a2, uint32_t w, uint32_t h, uint32_t bpp, rage::grcRenderTargetDesc* d) -> rage::grcRenderTargetPC*
    {
        auto rt = rage::grcTextureFactory::GetInstance()->CreateRenderTarget(name, a2, w, h, bpp, d);
        rage::grcDevice::grcResolveFlags resolveFlags{};
        rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, rt, nullptr);
        rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);
        return rt;
    };

    desc.mFormat = rage::GRCFMT_A8R8G8B8;
    auto rainRT = CreateEmptyRT("WaterDropsRT", 3, d3dsDesc.Width, d3dsDesc.Height, 32, &desc);
    WaterDrops::ms_tex = rainRT ? rainRT->mD3DTexture : nullptr;

    if (WaterDrops::ms_tex)
        WaterDrops::ms_tex->GetSurfaceLevel(0, &WaterDrops::ms_surf);

    WaterDrops::ms_fbWidth = d3dsDesc.Width;
    WaterDrops::ms_fbHeight = d3dsDesc.Height;
    WaterDrops::ms_scaling = WaterDrops::ms_fbHeight / 480.0f;

    // Mask texture
    HRESULT res = NULL;
    HMODULE hm = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&WaterDrops__InitialiseRender, &hm);
    res = D3DXCreateTextureFromResource(pDevice, hm, MAKEINTRESOURCE(IDR_DROPMASK), &WaterDrops::ms_maskTex);

    if (FAILED(res) || WaterDrops::ms_maskTex == nullptr)
    {
        static constexpr auto MaskSize = 128;
        if (FAILED(D3DXCreateTexture(pDevice, MaskSize, MaskSize, 1, D3DUSAGE_WRITEONLY, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &WaterDrops::ms_maskTex)))
            D3DXCreateTexture(pDevice, MaskSize, MaskSize, 1, D3DUSAGE_WRITEONLY, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &WaterDrops::ms_maskTex);

        D3DLOCKED_RECT LockedRect;
        WaterDrops::ms_maskTex->LockRect(0, &LockedRect, NULL, 0);
        uint8_t* pixels = (uint8_t*)LockedRect.pBits;
        int32_t stride = LockedRect.Pitch;
        for (int y = 0; y < MaskSize; y++)
        {
            float yf = ((y + 0.5f) / MaskSize - 0.5f) * 2.0f;
            for (int x = 0; x < MaskSize; x++)
            {
                float xf = ((x + 0.5f) / MaskSize - 0.5f) * 2.0f;
                memset(&pixels[y * stride + x * 4], xf * xf + yf * yf < 1.0f ? 0xFF : 0x00, 4);
            }
        }
        WaterDrops::ms_maskTex->UnlockRect(0);
        WaterDrops::ms_atlasUsed = false;
    }

    WaterDrops::ms_initialised = 1;
}

static inline void WaterDrops__Process(LPDIRECT3DDEVICE pDevice)
{
    if (!WaterDrops::fTimeStep)
    {
        static std::list<int> m_times;
        LARGE_INTEGER frequency;
        LARGE_INTEGER time;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&time);

        if (m_times.size() == 50)
            m_times.pop_front();
        m_times.push_back(static_cast<int>(time.QuadPart));

        if (m_times.size() >= 2)
            WaterDrops::fps = static_cast<uint32_t>(0.5f + (static_cast<float>(m_times.size() - 1) *
                                                    static_cast<float>(frequency.QuadPart)) / static_cast<float>(m_times.back() - m_times.front()));
    }
    if (!WaterDrops::ms_initialised)
        WaterDrops__InitialiseRender(pDevice);
    WaterDrops::ProcessGlobalEmitters();
    WaterDrops::CalculateMovement();
    WaterDrops::SprayDrops();
    WaterDrops::ProcessMoving();
    WaterDrops::Fade();
}

void RegisterFountains()
{
    WaterDrops::RegisterGlobalEmitter({ -234.365f, 769.001f, 6.83f }, 10.0f); //middle park fountain
}

float* CWeatherRain = nullptr;
void __fastcall sub_B870A0(uint8_t* self, void* edx)
{
    if (self[0])
    {
        if (WaterDrops::bBloodDrops)
            WaterDrops::FillScreenMoving(50.0f, true); //blood_cam
        self[0] = 0;
    }
    if (self[12])
    {
        bool bNeedsToDisableDrops = false;
        static auto ff = GetModuleHandleW(L"GTAIV.EFLC.FusionFix.asi");
        if (ff)
        {
            static auto IsSnowEnabled = (bool(*)())GetProcAddress(ff, "IsSnowEnabled");
            static auto IsWeatherSnow = (bool(*)())GetProcAddress(ff, "IsWeatherSnow");
            if (IsSnowEnabled && IsWeatherSnow)
            {
                if (IsSnowEnabled() && IsWeatherSnow())
                    bNeedsToDisableDrops = true;
            }
        }

        if (!bNeedsToDisableDrops)
            WaterDrops::ms_rainIntensity = *CWeatherRain; //rain_drops
        self[12] = 0;
    }
    if (self[28])
    {
        WaterDrops::FillScreenMoving(1.0f); //water_splash_cam
        self[28] = 0;
    }
}

void Init()
{
    WaterDrops::ReadIniSettings();

    RegisterFountains();

    auto pattern = find_pattern("83 3D ? ? ? ? ? 74 17 8B 4D 14", "83 3D ? ? ? ? ? 74 15 8B 44 24 1C", "83 3D ? ? ? ? ? 74 EF");
    rage::grcDevice::ms_pD3DDevice = *pattern.get_first<IDirect3DDevice9**>(2);

    pattern = find_pattern("A3 ? ? ? ? E8 ? ? ? ? 83 EC 0C", "A3 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 5E");
    rage::grcTextureFactory::g_pTextureFactory = *pattern.get_first<rage::grcTextureFactoryPC**>(1);

    pattern = hook::pattern("F3 0F 11 05 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? F3 0F 11 0D");
    if (pattern.empty())
        pattern = hook::pattern("F3 0F 11 05 ? ? ? ? E8 ? ? ? ? 84 C0 74 15 E8 ? ? ? ? 84 C0");
    CWeatherRain = *pattern.get_first<float*>(4);

    //enable original rain drops render when camera not looking up
    pattern = hook::pattern("76 15 B3 01");
    if (pattern.empty())
    {
        pattern = hook::pattern("74 16 33 C0 80 7C 06");
        if (!pattern.empty())
        {
            injector::MakeNOP(pattern.get_first(-9), 3);
            injector::WriteMemory<uint16_t>(pattern.get_first(-9), 0x01B0, true); //mov al,01
        }
    }
    else
        injector::MakeNOP(pattern.get_first(0), 2);

    pattern = hook::pattern("56 8B F1 80 3E 00 74 08");
    injector::MakeJMP(pattern.get_first(0), sub_B870A0, true);

    static uint32_t pCamMatrix = 0;
    pattern = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 84 C0 75 13 6A 01 6A 01 68 F4 01 00 00 B9 ? ? ? ? E8 ? ? ? ? 57");
    if (pattern.empty())
    {
        pattern = hook::pattern("7A 21 B9");
        pCamMatrix = *pattern.get_first<uint32_t>(3);
    }
    else
        pCamMatrix = *pattern.get_first<uint32_t>(1);

    static auto ppDevice = *hook::get_pattern<uint32_t>("83 C4 0C A1 ? ? ? ? A3", 4);

    pattern = hook::pattern("A2 ? ? ? ? E8 ? ? ? ? 8B CE E8 ? ? ? ? 8B CE E8 ? ? ? ? 5E C2 04 00");
    static auto sub_8297D0 = injector::GetBranchDestination(pattern.get_first(19));
    struct CPostFXHook
    {
        void operator()(injector::reg_pack& regs)
        {
            injector::cstd<void()>::call(sub_8297D0);

            auto pDevice = *(LPDIRECT3DDEVICE9*)ppDevice;

            auto right = *(RwV3d*)(pCamMatrix + 0x00);
            auto up = *(RwV3d*)(pCamMatrix + 0x10);
            auto at = *(RwV3d*)(pCamMatrix + 0x20);
            auto pos = *(RwV3d*)(pCamMatrix + 0x30);

            WaterDrops::right.x = -right.x;
            WaterDrops::right.y = -right.y;
            WaterDrops::right.z = -right.z;
            WaterDrops::up = up;
            WaterDrops::at = at;
            WaterDrops::pos = pos;

            WaterDrops__Process(pDevice);
            WaterDrops::Render(pDevice);
            WaterDrops::ms_rainIntensity = 0.0f;
        }
    }; injector::MakeInline<CPostFXHook>(pattern.get_first(19));

    pattern = hook::pattern("B8 ? ? ? ? B9 ? ? ? ? BF ? ? ? ? F3 AB");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = 0xFFFFFFFE;

            if (ms_rainRT)
            {
                ms_rainRT->Destroy();
                ms_rainRT = nullptr;
            }

            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0));

    pattern = hook::pattern("D8 0D ? ? ? ? 83 C0 30");
    if (pattern.empty())
    {
        pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 8B 02 51");
        WaterDrops::fTimeStep = *pattern.get_first<float*>(4);
    }
    else
        WaterDrops::fTimeStep = *pattern.get_first<float*>(-9);
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init);
    });
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent()) { InitializeASI(); }
    }
    return TRUE;
}