#ifndef _MINIDX8_H
#define _MINIDX8_H
#define COM_NO_WINDOWS_H
#include <objbase.h>

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |       \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif /* defined(MAKEFOURCC) */

typedef enum _D3DRESOURCETYPE_D3D8 {
    D3D8_D3DRTYPE_SURFACE = 1,
    D3D8_D3DRTYPE_VOLUME = 2,
    D3D8_D3DRTYPE_TEXTURE = 3,
    D3D8_D3DRTYPE_VOLUMETEXTURE = 4,
    D3D8_D3DRTYPE_CUBETEXTURE = 5,
    D3D8_D3DRTYPE_VERTEXBUFFER = 6,
    D3D8_D3DRTYPE_INDEXBUFFER = 7,
    D3D8_D3DRTYPE_FORCE_DWORD = 0x7fffffff
} D3DRESOURCETYPE_D3D8;

typedef enum _D3DDEVTYPE_D3D8
{
    D3D8_D3DDEVTYPE_HAL = 1,
    D3D8_D3DDEVTYPE_REF = 2,
    D3D8_D3DDEVTYPE_SW = 3,
    D3D8_D3DDEVTYPE_FORCE_DWORD = 0x7fffffff
} D3DDEVTYPE_D3D8;

typedef enum _D3DMULTISAMPLE_TYPE_D3D8
{
    D3D8_D3DMULTISAMPLE_NONE = 0,
    D3D8_D3DMULTISAMPLE_2_SAMPLES = 2,
    D3D8_D3DMULTISAMPLE_3_SAMPLES = 3,
    D3D8_D3DMULTISAMPLE_4_SAMPLES = 4,
    D3D8_D3DMULTISAMPLE_5_SAMPLES = 5,
    D3D8_D3DMULTISAMPLE_6_SAMPLES = 6,
    D3D8_D3DMULTISAMPLE_7_SAMPLES = 7,
    D3D8_D3DMULTISAMPLE_8_SAMPLES = 8,
    D3D8_D3DMULTISAMPLE_9_SAMPLES = 9,
    D3D8_D3DMULTISAMPLE_10_SAMPLES = 10,
    D3D8_D3DMULTISAMPLE_11_SAMPLES = 11,
    D3D8_D3DMULTISAMPLE_12_SAMPLES = 12,
    D3D8_D3DMULTISAMPLE_13_SAMPLES = 13,
    D3D8_D3DMULTISAMPLE_14_SAMPLES = 14,
    D3D8_D3DMULTISAMPLE_15_SAMPLES = 15,
    D3D8_D3DMULTISAMPLE_16_SAMPLES = 16,
    D3D8_D3DMULTISAMPLE_FORCE_DWORD = 0x7fffffff
} D3DMULTISAMPLE_TYPE_D3D8;

typedef enum _D3DFORMAT_D3D8
{
    D3D8_D3DFMT_UNKNOWN = 0,
    D3D8_D3DFMT_R8G8B8 = 20,
    D3D8_D3DFMT_A8R8G8B8 = 21,
    D3D8_D3DFMT_X8R8G8B8 = 22,
    D3D8_D3DFMT_R5G6B5 = 23,
    D3D8_D3DFMT_X1R5G5B5 = 24,
    D3D8_D3DFMT_A1R5G5B5 = 25,
    D3D8_D3DFMT_A4R4G4B4 = 26,
    D3D8_D3DFMT_R3G3B2 = 27,
    D3D8_D3DFMT_A8 = 28,
    D3D8_D3DFMT_A8R3G3B2 = 29,
    D3D8_D3DFMT_X4R4G4B4 = 30,
    D3D8_D3DFMT_A2B10G10R10 = 31,
    D3D8_D3DFMT_G16R16 = 34,
    D3D8_D3DFMT_A8P8 = 40,
    D3D8_D3DFMT_P8 = 41,
    D3D8_D3DFMT_L8 = 50,
    D3D8_D3DFMT_A8L8 = 51,
    D3D8_D3DFMT_A4L4 = 52,
    D3D8_D3DFMT_V8U8 = 60,
    D3D8_D3DFMT_L6V5U5 = 61,
    D3D8_D3DFMT_X8L8V8U8 = 62,
    D3D8_D3DFMT_Q8W8V8U8 = 63,
    D3D8_D3DFMT_V16U16 = 64,
    D3D8_D3DFMT_W11V11U10 = 65,
    D3D8_D3DFMT_A2W10V10U10 = 67,
    D3D8_D3DFMT_UYVY = MAKEFOURCC('U', 'Y', 'V', 'Y'),
    D3D8_D3DFMT_YUY2 = MAKEFOURCC('Y', 'U', 'Y', '2'),
    D3D8_D3DFMT_DXT1 = MAKEFOURCC('D', 'X', 'T', '1'),
    D3D8_D3DFMT_DXT2 = MAKEFOURCC('D', 'X', 'T', '2'),
    D3D8_D3DFMT_DXT3 = MAKEFOURCC('D', 'X', 'T', '3'),
    D3D8_D3DFMT_DXT4 = MAKEFOURCC('D', 'X', 'T', '4'),
    D3D8_D3DFMT_DXT5 = MAKEFOURCC('D', 'X', 'T', '5'),
    D3D8_D3DFMT_D16_LOCKABLE = 70,
    D3D8_D3DFMT_D32 = 71,
    D3D8_D3DFMT_D15S1 = 73,
    D3D8_D3DFMT_D24S8 = 75,
    D3D8_D3DFMT_D16 = 80,
    D3D8_D3DFMT_D24X8 = 77,
    D3D8_D3DFMT_D24X4S4 = 79,
    D3D8_D3DFMT_VERTEXDATA = 100,
    D3D8_D3DFMT_INDEX16 = 101,
    D3D8_D3DFMT_INDEX32 = 102,
    D3D8_D3DFMT_FORCE_DWORD = 0x7fffffff
} D3DFORMAT_D3D8;

#undef MAKEFOURCC

typedef enum _D3DSWAPEFFECT_D3D8
{
    D3D8_D3DSWAPEFFECT_DISCARD = 1,
    D3D8_D3DSWAPEFFECT_FLIP = 2,
    D3D8_D3DSWAPEFFECT_COPY = 3,
    D3D8_D3DSWAPEFFECT_COPY_VSYNC = 4,
    D3D8_D3DSWAPEFFECT_FORCE_DWORD = 0x7fffffff
} D3DSWAPEFFECT_D3D8;

typedef struct _D3DPRESENT_PARAMETERS_D3D8
{
    UINT                BackBufferWidth;
    UINT                BackBufferHeight;
    D3DFORMAT_D3D8      BackBufferFormat;
    UINT                BackBufferCount;

    D3DMULTISAMPLE_TYPE_D3D8 MultiSampleType;

    D3DSWAPEFFECT_D3D8  SwapEffect;
    HWND                hDeviceWindow;
    BOOL                Windowed;
    BOOL                EnableAutoDepthStencil;
    D3DFORMAT_D3D8           AutoDepthStencilFormat;
    DWORD               Flags;

    /* Following elements must be zero for Windowed mode */
    UINT                FullScreen_RefreshRateInHz;
    UINT                FullScreen_PresentationInterval;

} D3DPRESENT_PARAMETERS_D3D8;

typedef enum _D3DPOOL_D3D8 {
    D3D8_D3DPOOL_DEFAULT = 0,
    D3D8_D3DPOOL_MANAGED = 1,
    D3D8_D3DPOOL_SYSTEMMEM = 2,
    D3D8_D3DPOOL_SCRATCH = 3,
    D3D8_D3DPOOL_FORCE_DWORD = 0x7fffffff
} D3DPOOL_D3D8;

typedef enum _D3DBACKBUFFER_TYPE_D3D8
{
    D3D8_D3DBACKBUFFER_TYPE_MONO = 0,
    D3D8_D3DBACKBUFFER_TYPE_LEFT = 1,
    D3D8_D3DBACKBUFFER_TYPE_RIGHT = 2,
    D3D8_D3DBACKBUFFER_TYPE_FORCE_DWORD = 0x7fffffff
} D3DBACKBUFFER_TYPE_D3D8;

typedef enum _D3DTRANSFORMSTATETYPE_D3D8 {
    D3D8_D3DTS_VIEW = 2,
    D3D8_D3DTS_PROJECTION = 3,
    D3D8_D3DTS_TEXTURE0 = 16,
    D3D8_D3DTS_TEXTURE1 = 17,
    D3D8_D3DTS_TEXTURE2 = 18,
    D3D8_D3DTS_TEXTURE3 = 19,
    D3D8_D3DTS_TEXTURE4 = 20,
    D3D8_D3DTS_TEXTURE5 = 21,
    D3D8_D3DTS_TEXTURE6 = 22,
    D3D8_D3DTS_TEXTURE7 = 23,
    D3D8_D3DTS_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
} D3DTRANSFORMSTATETYPE_D3D8;

typedef enum _D3DRENDERSTATETYPE_D3D8 {
    D3D8_D3DRS_ZENABLE = 7,    /* D3DZBUFFERTYPE (or TRUE/FALSE for legacy) */
    D3D8_D3DRS_FILLMODE = 8,    /* D3DFILLMODE */
    D3D8_D3DRS_SHADEMODE = 9,    /* D3DSHADEMODE */
    D3D8_D3DRS_LINEPATTERN = 10,   /* D3DLINEPATTERN */
    D3D8_D3DRS_ZWRITEENABLE = 14,   /* TRUE to enable z writes */
    D3D8_D3DRS_ALPHATESTENABLE = 15,   /* TRUE to enable alpha tests */
    D3D8_D3DRS_LASTPIXEL = 16,   /* TRUE for last-pixel on lines */
    D3D8_D3DRS_SRCBLEND = 19,   /* D3DBLEND */
    D3D8_D3DRS_DESTBLEND = 20,   /* D3DBLEND */
    D3D8_D3DRS_CULLMODE = 22,   /* D3DCULL */
    D3D8_D3DRS_ZFUNC = 23,   /* D3DCMPFUNC */
    D3D8_D3DRS_ALPHAREF = 24,   /* D3DFIXED */
    D3D8_D3DRS_ALPHAFUNC = 25,   /* D3DCMPFUNC */
    D3D8_D3DRS_DITHERENABLE = 26,   /* TRUE to enable dithering */
    D3D8_D3DRS_ALPHABLENDENABLE = 27,   /* TRUE to enable alpha blending */
    D3D8_D3DRS_FOGENABLE = 28,   /* TRUE to enable fog blending */
    D3D8_D3DRS_SPECULARENABLE = 29,   /* TRUE to enable specular */
    D3D8_D3DRS_ZVISIBLE = 30,   /* TRUE to enable z checking */
    D3D8_D3DRS_FOGCOLOR = 34,   /* D3DCOLOR */
    D3D8_D3DRS_FOGTABLEMODE = 35,   /* D3DFOGMODE */
    D3D8_D3DRS_FOGSTART = 36,   /* Fog start (for both vertex and pixel fog) */
    D3D8_D3DRS_FOGEND = 37,   /* Fog end      */
    D3D8_D3DRS_FOGDENSITY = 38,   /* Fog density  */
    D3D8_D3DRS_EDGEANTIALIAS = 40,   /* TRUE to enable edge antialiasing */
    D3D8_D3DRS_ZBIAS = 47,   /* LONG Z bias */
    D3D8_D3DRS_RANGEFOGENABLE = 48,   /* Enables range-based fog */
    D3D8_D3DRS_STENCILENABLE = 52,   /* BOOL enable/disable stenciling */
    D3D8_D3DRS_STENCILFAIL = 53,   /* D3DSTENCILOP to do if stencil test fails */
    D3D8_D3DRS_STENCILZFAIL = 54,   /* D3DSTENCILOP to do if stencil test passes and Z test fails */
    D3D8_D3DRS_STENCILPASS = 55,   /* D3DSTENCILOP to do if both stencil and Z tests pass */
    D3D8_D3DRS_STENCILFUNC = 56,   /* D3DCMPFUNC fn.  Stencil Test passes if ((ref & mask) stencilfn (stencil & mask)) is true */
    D3D8_D3DRS_STENCILREF = 57,   /* Reference value used in stencil test */
    D3D8_D3DRS_STENCILMASK = 58,   /* Mask value used in stencil test */
    D3D8_D3DRS_STENCILWRITEMASK = 59,   /* Write mask applied to values written to stencil buffer */
    D3D8_D3DRS_TEXTUREFACTOR = 60,   /* D3DCOLOR used for multi-texture blend */
    D3D8_D3DRS_WRAP0 = 128,  /* wrap for 1st texture coord. set */
    D3D8_D3DRS_WRAP1 = 129,  /* wrap for 2nd texture coord. set */
    D3D8_D3DRS_WRAP2 = 130,  /* wrap for 3rd texture coord. set */
    D3D8_D3DRS_WRAP3 = 131,  /* wrap for 4th texture coord. set */
    D3D8_D3DRS_WRAP4 = 132,  /* wrap for 5th texture coord. set */
    D3D8_D3DRS_WRAP5 = 133,  /* wrap for 6th texture coord. set */
    D3D8_D3DRS_WRAP6 = 134,  /* wrap for 7th texture coord. set */
    D3D8_D3DRS_WRAP7 = 135,  /* wrap for 8th texture coord. set */
    D3D8_D3DRS_CLIPPING = 136,
    D3D8_D3DRS_LIGHTING = 137,
    D3D8_D3DRS_AMBIENT = 139,
    D3D8_D3DRS_FOGVERTEXMODE = 140,
    D3D8_D3DRS_COLORVERTEX = 141,
    D3D8_D3DRS_LOCALVIEWER = 142,
    D3D8_D3DRS_NORMALIZENORMALS = 143,
    D3D8_D3DRS_DIFFUSEMATERIALSOURCE = 145,
    D3D8_D3DRS_SPECULARMATERIALSOURCE = 146,
    D3D8_D3DRS_AMBIENTMATERIALSOURCE = 147,
    D3D8_D3DRS_EMISSIVEMATERIALSOURCE = 148,
    D3D8_D3DRS_VERTEXBLEND = 151,
    D3D8_D3DRS_CLIPPLANEENABLE = 152,
    D3D8_D3DRS_SOFTWAREVERTEXPROCESSING = 153,
    D3D8_D3DRS_POINTSIZE = 154,   /* float point size */
    D3D8_D3DRS_POINTSIZE_MIN = 155,   /* float point size min threshold */
    D3D8_D3DRS_POINTSPRITEENABLE = 156,   /* BOOL point texture coord control */
    D3D8_D3DRS_POINTSCALEENABLE = 157,   /* BOOL point size scale enable */
    D3D8_D3DRS_POINTSCALE_A = 158,   /* float point attenuation A value */
    D3D8_D3DRS_POINTSCALE_B = 159,   /* float point attenuation B value */
    D3D8_D3DRS_POINTSCALE_C = 160,   /* float point attenuation C value */
    D3D8_D3DRS_MULTISAMPLEANTIALIAS = 161,  // BOOL - set to do FSAA with multisample buffer
    D3D8_D3DRS_MULTISAMPLEMASK = 162,  // DWORD - per-sample enable/disable
    D3D8_D3DRS_PATCHEDGESTYLE = 163,  // Sets whether patch edges will use float style tessellation
    D3D8_D3DRS_PATCHSEGMENTS = 164,  // Number of segments per edge when drawing patches
    D3D8_D3DRS_DEBUGMONITORTOKEN = 165,  // DEBUG ONLY - token to debug monitor
    D3D8_D3DRS_POINTSIZE_MAX = 166,   /* float point size max threshold */
    D3D8_D3DRS_INDEXEDVERTEXBLENDENABLE = 167,
    D3D8_D3DRS_COLORWRITEENABLE = 168,  // per-channel write enable
    D3D8_D3DRS_TWEENFACTOR = 170,   // float tween factor
    D3D8_D3DRS_BLENDOP = 171,   // D3DBLENDOP setting
    D3D8_D3DRS_POSITIONORDER = 172,   // NPatch position interpolation order. D3DORDER_LINEAR or D3DORDER_CUBIC (default)
    D3D8_D3DRS_NORMALORDER = 173,   // NPatch normal interpolation order. D3DORDER_LINEAR (default) or D3DORDER_QUADRATIC
    D3D8_D3DRS_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
} D3DRENDERSTATETYPE_D3D8;

typedef enum _D3DSTATEBLOCKTYPE_D3D8
{
    D3D8_D3DSBT_ALL = 1, // capture all state
    D3D8_D3DSBT_PIXELSTATE = 2, // capture pixel state
    D3D8_D3DSBT_VERTEXSTATE = 3, // capture vertex state
    D3D8_D3DSBT_FORCE_DWORD = 0x7fffffff,
} D3DSTATEBLOCKTYPE_D3D8;

typedef enum _D3DTEXTURESTAGESTATETYPE_D3D8
{
    D3D8_D3DTSS_COLOROP = 1, /* D3DTEXTUREOP - per-stage blending controls for color channels */
    D3D8_D3DTSS_COLORARG1 = 2, /* D3DTA_* (texture arg) */
    D3D8_D3DTSS_COLORARG2 = 3, /* D3DTA_* (texture arg) */
    D3D8_D3DTSS_ALPHAOP = 4, /* D3DTEXTUREOP - per-stage blending controls for alpha channel */
    D3D8_D3DTSS_ALPHAARG1 = 5, /* D3DTA_* (texture arg) */
    D3D8_D3DTSS_ALPHAARG2 = 6, /* D3DTA_* (texture arg) */
    D3D8_D3DTSS_BUMPENVMAT00 = 7, /* float (bump mapping matrix) */
    D3D8_D3DTSS_BUMPENVMAT01 = 8, /* float (bump mapping matrix) */
    D3D8_D3DTSS_BUMPENVMAT10 = 9, /* float (bump mapping matrix) */
    D3D8_D3DTSS_BUMPENVMAT11 = 10, /* float (bump mapping matrix) */
    D3D8_D3DTSS_TEXCOORDINDEX = 11, /* identifies which set of texture coordinates index this texture */
    D3D8_D3DTSS_ADDRESSU = 13, /* D3DTEXTUREADDRESS for U coordinate */
    D3D8_D3DTSS_ADDRESSV = 14, /* D3DTEXTUREADDRESS for V coordinate */
    D3D8_D3DTSS_BORDERCOLOR = 15, /* D3DCOLOR */
    D3D8_D3DTSS_MAGFILTER = 16, /* D3DTEXTUREFILTER filter to use for magnification */
    D3D8_D3DTSS_MINFILTER = 17, /* D3DTEXTUREFILTER filter to use for minification */
    D3D8_D3DTSS_MIPFILTER = 18, /* D3DTEXTUREFILTER filter to use between mipmaps during minification */
    D3D8_D3DTSS_MIPMAPLODBIAS = 19, /* float Mipmap LOD bias */
    D3D8_D3DTSS_MAXMIPLEVEL = 20, /* DWORD 0..(n-1) LOD index of largest map to use (0 == largest) */
    D3D8_D3DTSS_MAXANISOTROPY = 21, /* DWORD maximum anisotropy */
    D3D8_D3DTSS_BUMPENVLSCALE = 22, /* float scale for bump map luminance */
    D3D8_D3DTSS_BUMPENVLOFFSET = 23, /* float offset for bump map luminance */
    D3D8_D3DTSS_TEXTURETRANSFORMFLAGS = 24, /* D3DTEXTURETRANSFORMFLAGS controls texture transform */
    D3D8_D3DTSS_ADDRESSW = 25, /* D3DTEXTUREADDRESS for W coordinate */
    D3D8_D3DTSS_COLORARG0 = 26, /* D3DTA_* third arg for triadic ops */
    D3D8_D3DTSS_ALPHAARG0 = 27, /* D3DTA_* third arg for triadic ops */
    D3D8_D3DTSS_RESULTARG = 28, /* D3DTA_* arg for result (CURRENT or TEMP) */
    D3D8_D3DTSS_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
} D3DTEXTURESTAGESTATETYPE_D3D8;

typedef enum _D3DPRIMITIVETYPE_D3D8 {
    D3D8_D3DPT_POINTLIST = 1,
    D3D8_D3DPT_LINELIST = 2,
    D3D8_D3DPT_LINESTRIP = 3,
    D3D8_D3DPT_TRIANGLELIST = 4,
    D3D8_D3DPT_TRIANGLESTRIP = 5,
    D3D8_D3DPT_TRIANGLEFAN = 6,
    D3D8_D3DPT_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
} D3DPRIMITIVETYPE_D3D8;

typedef enum _D3DBASISTYPE_D3D8 {
    D3D8_D3DBASIS_BEZIER = 0,
    D3D8_D3DBASIS_BSPLINE = 1,
    D3D8_D3DBASIS_INTERPOLATE = 2,
    D3D8_D3DBASIS_FORCE_DWORD = 0x7fffffff,
} D3DBASISTYPE_D3D8;

typedef enum _D3DORDERTYPE_D3D8 {
    D3D8_D3DORDER_LINEAR = 1,
    D3D8_D3DORDER_QUADRATIC = 2,
    D3D8_D3DORDER_CUBIC = 3,
    D3D8_D3DORDER_QUINTIC = 5,
    D3D8_D3DORDER_FORCE_DWORD = 0x7fffffff,
} D3DORDERTYPE_D3D8;

typedef struct _D3DRECTPATCH_INFO_D3D8
{
    UINT                StartVertexOffsetWidth;
    UINT                StartVertexOffsetHeight;
    UINT                Width;
    UINT                Height;
    UINT                Stride;
    D3DBASISTYPE_D3D8   Basis;
    D3DORDERTYPE_D3D8   Order;
} D3DRECTPATCH_INFO_D3D8;

typedef struct _D3DTRIPATCH_INFO_D3D8
{
    UINT                StartVertexOffset;
    UINT                NumVertices;
    D3DBASISTYPE_D3D8   Basis;
    D3DORDERTYPE_D3D8   Order;
} D3DTRIPATCH_INFO_D3D8;

typedef struct _D3DDISPLAYMODE_D3D8
{
    UINT            Width;
    UINT            Height;
    UINT            RefreshRate;
    D3DFORMAT_D3D8  Format;
} D3DDISPLAYMODE_D3D8;

DECLARE_INTERFACE_(D3D8_IDirect3D8, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /*** IDirect3D8 methods ***/
    STDMETHOD(RegisterSoftwareDevice)(THIS_ void* pInitializeFunction) PURE;
    STDMETHOD_(UINT, GetAdapterCount)(THIS) PURE;
    STDMETHOD(GetAdapterIdentifier)(THIS_ UINT Adapter, DWORD Flags, VOID * pIdentifier) PURE;
    STDMETHOD_(UINT, GetAdapterModeCount)(THIS_ UINT Adapter) PURE;
    STDMETHOD(EnumAdapterModes)(THIS_ UINT Adapter, UINT Mode, VOID * pMode) PURE;
    STDMETHOD(GetAdapterDisplayMode)(THIS_ UINT Adapter, VOID * pMode) PURE;
    STDMETHOD(CheckDeviceType)(THIS_ UINT Adapter, D3DDEVTYPE_D3D8 CheckType, D3DFORMAT_D3D8 DisplayFormat, D3DFORMAT_D3D8 BackBufferFormat, BOOL Windowed) PURE;
    STDMETHOD(CheckDeviceFormat)(THIS_ UINT Adapter, D3DDEVTYPE_D3D8 DeviceType, D3DFORMAT_D3D8 AdapterFormat, DWORD Usage, D3DRESOURCETYPE_D3D8 RType, D3DFORMAT_D3D8 CheckFormat) PURE;
    STDMETHOD(CheckDeviceMultiSampleType)(THIS_ UINT Adapter, D3DDEVTYPE_D3D8 DeviceType, D3DFORMAT_D3D8 SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE_D3D8 MultiSampleType) PURE;
    STDMETHOD(CheckDepthStencilMatch)(THIS_ UINT Adapter, D3DDEVTYPE_D3D8 DeviceType, D3DFORMAT_D3D8 AdapterFormat, D3DFORMAT_D3D8 RenderTargetFormat, D3DFORMAT_D3D8 DepthStencilFormat) PURE;
    STDMETHOD(GetDeviceCaps)(THIS_ UINT Adapter, D3DDEVTYPE_D3D8 DeviceType, VOID * pCaps) PURE;
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(THIS_ UINT Adapter) PURE;
    STDMETHOD(CreateDevice)(THIS_ UINT Adapter, D3DDEVTYPE_D3D8 DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS_D3D8 * pPresentationParameters, void** ppReturnedDeviceInterface) PURE;
};

typedef struct D3D8_IDirect3D8* D3D8_LPDIRECT3D8, * D3D8_PDIRECT3D8;

DECLARE_INTERFACE_(D3D8_IDirect3DDevice8, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /*** IDirect3DDevice8 methods ***/
    STDMETHOD(TestCooperativeLevel)(THIS) PURE;
    STDMETHOD_(UINT, GetAvailableTextureMem)(THIS) PURE;
    STDMETHOD(ResourceManagerDiscardBytes)(THIS_ DWORD Bytes) PURE;
    STDMETHOD(GetDirect3D)(THIS_ D3D8_IDirect3D8 * *ppD3D8) PURE;
    STDMETHOD(GetDeviceCaps)(THIS_ VOID * pCaps) PURE;
    STDMETHOD(GetDisplayMode)(THIS_ VOID * pMode) PURE;
    STDMETHOD(GetCreationParameters)(THIS_ VOID * pParameters) PURE;
    STDMETHOD(SetCursorProperties)(THIS_ UINT XHotSpot, UINT YHotSpot, VOID * pCursorBitmap) PURE;
    STDMETHOD_(void, SetCursorPosition)(THIS_ int X, int Y, DWORD Flags) PURE;
    STDMETHOD_(BOOL, ShowCursor)(THIS_ BOOL bShow) PURE;
    STDMETHOD(CreateAdditionalSwapChain)(THIS_ D3DPRESENT_PARAMETERS_D3D8 * pPresentationParameters, VOID * *pSwapChain) PURE;
    STDMETHOD(Reset)(THIS_ D3DPRESENT_PARAMETERS_D3D8 * pPresentationParameters) PURE;
    STDMETHOD(Present)(THIS_ CONST RECT * pSourceRect, CONST RECT * pDestRect, HWND hDestWindowOverride, CONST RGNDATA * pDirtyRegion) PURE;
    STDMETHOD(GetBackBuffer)(THIS_ UINT BackBuffer, D3DBACKBUFFER_TYPE_D3D8 Type, VOID * *ppBackBuffer) PURE;
    STDMETHOD(GetRasterStatus)(THIS_ VOID * pRasterStatus) PURE;
    STDMETHOD_(void, SetGammaRamp)(THIS_ DWORD Flags, CONST VOID * pRamp) PURE;
    STDMETHOD_(void, GetGammaRamp)(THIS_ VOID * pRamp) PURE;
    STDMETHOD(CreateTexture)(THIS_ UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT_D3D8 Format, D3DPOOL_D3D8 Pool, VOID * *ppTexture) PURE;
    STDMETHOD(CreateVolumeTexture)(THIS_ UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT_D3D8 Format, D3DPOOL_D3D8 Pool, VOID * *ppVolumeTexture) PURE;
    STDMETHOD(CreateCubeTexture)(THIS_ UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT_D3D8 Format, D3DPOOL_D3D8 Pool, VOID * *ppCubeTexture) PURE;
    STDMETHOD(CreateVertexBuffer)(THIS_ UINT Length, DWORD Usage, DWORD FVF, D3DPOOL_D3D8 Pool, VOID * *ppVertexBuffer) PURE;
    STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length, DWORD Usage, D3DFORMAT_D3D8 Format, D3DPOOL_D3D8 Pool, VOID * *ppIndexBuffer) PURE;
    STDMETHOD(CreateRenderTarget)(THIS_ UINT Width, UINT Height, D3DFORMAT_D3D8 Format, D3DMULTISAMPLE_TYPE_D3D8 MultiSample, BOOL Lockable, VOID * *ppSurface) PURE;
    STDMETHOD(CreateDepthStencilSurface)(THIS_ UINT Width, UINT Height, D3DFORMAT_D3D8 Format, D3DMULTISAMPLE_TYPE_D3D8 MultiSample, VOID * *ppSurface) PURE;
    STDMETHOD(CreateImageSurface)(THIS_ UINT Width, UINT Height, D3DFORMAT_D3D8 Format, VOID * *ppSurface) PURE;
    STDMETHOD(CopyRects)(THIS_ VOID * pSourceSurface, CONST RECT * pSourceRectsArray, UINT cRects, VOID * pDestinationSurface, CONST POINT * pDestPointsArray) PURE;
    STDMETHOD(UpdateTexture)(THIS_ VOID * pSourceTexture, VOID * pDestinationTexture) PURE;
    STDMETHOD(GetFrontBuffer)(THIS_ VOID * pDestSurface) PURE;
    STDMETHOD(SetRenderTarget)(THIS_ VOID * pRenderTarget, VOID * pNewZStencil) PURE;
    STDMETHOD(GetRenderTarget)(THIS_ VOID * *ppRenderTarget) PURE;
    STDMETHOD(GetDepthStencilSurface)(THIS_ VOID * *ppZStencilSurface) PURE;
    STDMETHOD(BeginScene)(THIS) PURE;
    STDMETHOD(EndScene)(THIS) PURE;
    STDMETHOD(Clear)(THIS_ DWORD Count, CONST VOID * pRects, DWORD Flags, DWORD Color, float Z, DWORD Stencil) PURE;
    STDMETHOD(SetTransform)(THIS_ D3DTRANSFORMSTATETYPE_D3D8 State, CONST VOID * pMatrix) PURE;
    STDMETHOD(GetTransform)(THIS_ D3DTRANSFORMSTATETYPE_D3D8 State, VOID * pMatrix) PURE;
    STDMETHOD(MultiplyTransform)(THIS_ D3DTRANSFORMSTATETYPE_D3D8, CONST VOID*) PURE;
    STDMETHOD(SetViewport)(THIS_ CONST VOID * pViewport) PURE;
    STDMETHOD(GetViewport)(THIS_ VOID * pViewport) PURE;
    STDMETHOD(SetMaterial)(THIS_ CONST VOID * pMaterial) PURE;
    STDMETHOD(GetMaterial)(THIS_ VOID * pMaterial) PURE;
    STDMETHOD(SetLight)(THIS_ DWORD Index, CONST VOID*) PURE;
    STDMETHOD(GetLight)(THIS_ DWORD Index, VOID*) PURE;
    STDMETHOD(LightEnable)(THIS_ DWORD Index, BOOL Enable) PURE;
    STDMETHOD(GetLightEnable)(THIS_ DWORD Index, BOOL * pEnable) PURE;
    STDMETHOD(SetClipPlane)(THIS_ DWORD Index, CONST float* pPlane) PURE;
    STDMETHOD(GetClipPlane)(THIS_ DWORD Index, float* pPlane) PURE;
    STDMETHOD(SetRenderState)(THIS_ D3DRENDERSTATETYPE_D3D8 State, DWORD Value) PURE;
    STDMETHOD(GetRenderState)(THIS_ D3DRENDERSTATETYPE_D3D8 State, DWORD * pValue) PURE;
    STDMETHOD(BeginStateBlock)(THIS) PURE;
    STDMETHOD(EndStateBlock)(THIS_ DWORD * pToken) PURE;
    STDMETHOD(ApplyStateBlock)(THIS_ DWORD Token) PURE;
    STDMETHOD(CaptureStateBlock)(THIS_ DWORD Token) PURE;
    STDMETHOD(DeleteStateBlock)(THIS_ DWORD Token) PURE;
    STDMETHOD(CreateStateBlock)(THIS_ D3DSTATEBLOCKTYPE_D3D8 Type, DWORD * pToken) PURE;
    STDMETHOD(SetClipStatus)(THIS_ CONST VOID * pClipStatus) PURE;
    STDMETHOD(GetClipStatus)(THIS_ VOID * pClipStatus) PURE;
    STDMETHOD(GetTexture)(THIS_ DWORD Stage, VOID * *ppTexture) PURE;
    STDMETHOD(SetTexture)(THIS_ DWORD Stage, VOID * pTexture) PURE;
    STDMETHOD(GetTextureStageState)(THIS_ DWORD Stage, D3DTEXTURESTAGESTATETYPE_D3D8 Type, DWORD * pValue) PURE;
    STDMETHOD(SetTextureStageState)(THIS_ DWORD Stage, D3DTEXTURESTAGESTATETYPE_D3D8 Type, DWORD Value) PURE;
    STDMETHOD(ValidateDevice)(THIS_ DWORD * pNumPasses) PURE;
    STDMETHOD(GetInfo)(THIS_ DWORD DevInfoID, void* pDevInfoStruct, DWORD DevInfoStructSize) PURE;
    STDMETHOD(SetPaletteEntries)(THIS_ UINT PaletteNumber, CONST PALETTEENTRY * pEntries) PURE;
    STDMETHOD(GetPaletteEntries)(THIS_ UINT PaletteNumber, PALETTEENTRY * pEntries) PURE;
    STDMETHOD(SetCurrentTexturePalette)(THIS_ UINT PaletteNumber) PURE;
    STDMETHOD(GetCurrentTexturePalette)(THIS_ UINT * PaletteNumber) PURE;
    STDMETHOD(DrawPrimitive)(THIS_ D3DPRIMITIVETYPE_D3D8 PrimitiveType, UINT StartVertex, UINT PrimitiveCount) PURE;
    STDMETHOD(DrawIndexedPrimitive)(THIS_ D3DPRIMITIVETYPE_D3D8, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount) PURE;
    STDMETHOD(DrawPrimitiveUP)(THIS_ D3DPRIMITIVETYPE_D3D8 PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) PURE;
    STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ D3DPRIMITIVETYPE_D3D8 PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT_D3D8 IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) PURE;
    STDMETHOD(ProcessVertices)(THIS_ UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, VOID * pDestBuffer, DWORD Flags) PURE;
    STDMETHOD(CreateVertexShader)(THIS_ CONST DWORD * pDeclaration, CONST DWORD * pFunction, DWORD * pHandle, DWORD Usage) PURE;
    STDMETHOD(SetVertexShader)(THIS_ DWORD Handle) PURE;
    STDMETHOD(GetVertexShader)(THIS_ DWORD * pHandle) PURE;
    STDMETHOD(DeleteVertexShader)(THIS_ DWORD Handle) PURE;
    STDMETHOD(SetVertexShaderConstant)(THIS_ DWORD Register, CONST void* pConstantData, DWORD ConstantCount) PURE;
    STDMETHOD(GetVertexShaderConstant)(THIS_ DWORD Register, void* pConstantData, DWORD ConstantCount) PURE;
    STDMETHOD(GetVertexShaderDeclaration)(THIS_ DWORD Handle, void* pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(GetVertexShaderFunction)(THIS_ DWORD Handle, void* pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(SetStreamSource)(THIS_ UINT StreamNumber, VOID * pStreamData, UINT Stride) PURE;
    STDMETHOD(GetStreamSource)(THIS_ UINT StreamNumber, VOID * *ppStreamData, UINT * pStride) PURE;
    STDMETHOD(SetIndices)(THIS_ VOID * pIndexData, UINT BaseVertexIndex) PURE;
    STDMETHOD(GetIndices)(THIS_ VOID * *ppIndexData, UINT * pBaseVertexIndex) PURE;
    STDMETHOD(CreatePixelShader)(THIS_ CONST DWORD * pFunction, DWORD * pHandle) PURE;
    STDMETHOD(SetPixelShader)(THIS_ DWORD Handle) PURE;
    STDMETHOD(GetPixelShader)(THIS_ DWORD * pHandle) PURE;
    STDMETHOD(DeletePixelShader)(THIS_ DWORD Handle) PURE;
    STDMETHOD(SetPixelShaderConstant)(THIS_ DWORD Register, CONST void* pConstantData, DWORD ConstantCount) PURE;
    STDMETHOD(GetPixelShaderConstant)(THIS_ DWORD Register, void* pConstantData, DWORD ConstantCount) PURE;
    STDMETHOD(GetPixelShaderFunction)(THIS_ DWORD Handle, void* pData, DWORD * pSizeOfData) PURE;
    STDMETHOD(DrawRectPatch)(THIS_ UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO_D3D8 * pRectPatchInfo) PURE;
    STDMETHOD(DrawTriPatch)(THIS_ UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO_D3D8 * pTriPatchInfo) PURE;
    STDMETHOD(DeletePatch)(THIS_ UINT Handle) PURE;
};

typedef struct D3D8_IDirect3DDevice8* D3D8_LPDIRECT3DDEVICE8, * D3D8_PDIRECT3DDEVICE8;
#endif
