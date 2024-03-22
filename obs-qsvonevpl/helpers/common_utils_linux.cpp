#include "common_utils.hpp"

#include <cpuid.h>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define INTEL_VENDOR_ID 0x8086

extern "C" void util_cpuid(int cpuinfo[4], int level) {
  __get_cpuid(level, (unsigned int *)&cpuinfo[0], (unsigned int *)&cpuinfo[1],
              (unsigned int *)&cpuinfo[2], (unsigned int *)&cpuinfo[3]);
}

static const char *default_h264_device = nullptr;
static const char *default_hevc_device = nullptr;
static const char *default_av1_device = nullptr;
static const char *default_vp9_device = nullptr;

#define DEVICE_MGR_TYPE MFX_HANDLE_VA_DISPLAY
// This ends up at like 72 for 1440p@120 av1 may end up hitting this in
// practice?
#define MAX_ALLOCABLE_SURFACES 128

struct surface_info {
  VASurfaceID id;
  int32_t width, height;
  gs_texture_t *tex_y;
  gs_texture_t *tex_uv;
};

mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest *request,
                       mfxFrameAllocResponse *response) {
  if (request->Type &
      (MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
    return MFX_ERR_UNSUPPORTED;

  response->mids = (mfxMemId *)nullptr;
  response->NumFrameActual = 0;

  mfxSession *session = (mfxSession *)pthis;
  VADisplay display;
  mfxStatus sts = MFXVideoCORE_GetHandle(*session, DEVICE_MGR_TYPE, &display);
  MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

  // https://ffmpeg.org/doxygen/5.1/hwcontext__vaapi_8c_source.html#l00109
  // though earlier comments suggest the driver ignores rt_format so we could
  // choose whatever.
  unsigned int rt_format;
  int32_t pix_format;
  switch (request->Info.FourCC) {
  case MFX_FOURCC_P010:
    rt_format = VA_RT_FORMAT_YUV420_10;
    pix_format = VA_FOURCC_P010;
    break;
  case MFX_FOURCC_NV12:
  default:
    rt_format = VA_RT_FORMAT_YUV420;
    pix_format = VA_FOURCC_NV12;
    break;
  }

  int num_attrs = 2;
  VASurfaceAttrib attrs[2] = {
      {
          .type = VASurfaceAttribMemoryType,
          .flags = VA_SURFACE_ATTRIB_SETTABLE,
          .value =
              {
                  .type = VAGenericValueTypeInteger,
                  .value = {.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2},
              },
      },
      {
          .type = VASurfaceAttribPixelFormat,
          .flags = VA_SURFACE_ATTRIB_SETTABLE,
          .value =
              {
                  .type = VAGenericValueTypeInteger,
                  .value = {.i = (int)pix_format},
              },
      }};

  unsigned int num_surfaces = request->NumFrameSuggested;
  VASurfaceID temp_surfaces[MAX_ALLOCABLE_SURFACES] = {0};
  assert(num_surfaces < MAX_ALLOCABLE_SURFACES);
  VAStatus vasts;
  if ((vasts = vaCreateSurfaces(display, rt_format, request->Info.Width,
                                request->Info.Height, temp_surfaces,
                                num_surfaces, attrs, num_attrs)) !=
      VA_STATUS_SUCCESS) {
    blog(LOG_ERROR, "failed to create surfaces: %d", vasts);
    return MFX_ERR_MEMORY_ALLOC;
  }

  // Follow the ffmpeg trick and stuff our pointer at the end.
  mfxMemId *mids = (mfxMemId *)bmalloc(sizeof(mfxMemId) * num_surfaces + 1);
  struct surface_info *surfaces = (struct surface_info *)bmalloc(
      sizeof(struct surface_info) * num_surfaces);

  mids[num_surfaces] = surfaces; // stuff it
  for (uint64_t i = 0; i < num_surfaces; i++) {
    surfaces[i].id = temp_surfaces[i];
    surfaces[i].width = request->Info.Width;
    surfaces[i].height = request->Info.Height;
    mids[i] = &surfaces[i];

    VADRMPRIMESurfaceDescriptor surfDesc = {0};
    if (vaExportSurfaceHandle(
            display, surfaces[i].id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
            VA_EXPORT_SURFACE_READ_WRITE, &surfDesc) != VA_STATUS_SUCCESS)
      return MFX_ERR_MEMORY_ALLOC;

    obs_enter_graphics();
    // TODO: P010 format support
    assert(surfDesc.num_objects == 1);
    int fds[4] = {0};
    uint32_t strides[4] = {0};
    uint32_t offsets[4] = {0};
    uint64_t modifiers[4] = {0};
    fds[0] = surfDesc.objects[surfDesc.layers[0].object_index[0]].fd;
    fds[1] = surfDesc.objects[surfDesc.layers[1].object_index[0]].fd;
    strides[0] = surfDesc.layers[0].pitch[0];
    strides[1] = surfDesc.layers[1].pitch[0];
    offsets[0] = surfDesc.layers[0].offset[0];
    offsets[1] = surfDesc.layers[1].offset[0];
    modifiers[0] = surfDesc.objects[surfDesc.layers[0].object_index[0]]
                       .drm_format_modifier;
    modifiers[1] = surfDesc.objects[surfDesc.layers[1].object_index[0]]
                       .drm_format_modifier;

    surfaces[i].tex_y = gs_texture_create_from_dmabuf(
        surfDesc.width, surfDesc.height, surfDesc.layers[0].drm_format, GS_R8,
        1, fds, strides, offsets, modifiers);
    surfaces[i].tex_uv = gs_texture_create_from_dmabuf(
        surfDesc.width / 2, surfDesc.height, surfDesc.layers[1].drm_format,
        GS_R8G8, 1, fds + 1, strides + 1, offsets + 1, modifiers + 1);
    obs_leave_graphics();

    close(surfDesc.objects[surfDesc.layers[0].object_index[0]].fd);
    if (!surfaces[i].tex_y || !surfaces[i].tex_uv) {
      return MFX_ERR_MEMORY_ALLOC;
    }
  }

  response->mids = (mfxMemId *)mids;
  response->NumFrameActual = num_surfaces;
  return MFX_ERR_NONE;
}

mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle) {
  UNUSED_PARAMETER(pthis);
  if (NULL == handle)
    return MFX_ERR_INVALID_HANDLE;

  // Seemingly undocumented, but Pair format defined by
  // oneVPL-intel-gpu-intel-onevpl-23.1.0/_studio/mfx_lib/encode_hw/av1/linux/base/av1ehw_base_va_packer_lin.cpp
  mfxHDLPair *pPair = (mfxHDLPair *)handle;

  // Pointer to a VASurfaceID, will be dereferenced by the driver.
  pPair->first = &((struct surface_info *)mid)->id;
  pPair->second = 0;

  return MFX_ERR_NONE;
}

mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse *response) {
  if (response->mids == nullptr || response->NumFrameActual == 0)
    return MFX_ERR_NONE;

  mfxSession *session = (mfxSession *)pthis;
  VADisplay display;
  mfxStatus sts = MFXVideoCORE_GetHandle(*session, DEVICE_MGR_TYPE, &display);
  MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

  struct surface_info *surfs =
      (struct surface_info *)response->mids[response->NumFrameActual];
  VASurfaceID temp_surfaces[MAX_ALLOCABLE_SURFACES] = {0};
  obs_enter_graphics();
  for (int i = 0; i < response->NumFrameActual; i++) {
    temp_surfaces[i] = *(VASurfaceID *)response->mids[i];
    gs_texture_destroy(surfs[i].tex_y);
    gs_texture_destroy(surfs[i].tex_uv);
  }
  obs_leave_graphics();

  bfree(surfs);
  bfree(response->mids);
  if (vaDestroySurfaces(display, temp_surfaces, response->NumFrameActual) !=
      VA_STATUS_SUCCESS)
    return MFX_ERR_MEMORY_ALLOC;

  return MFX_ERR_NONE;
}

mfxStatus simple_copytex(mfxHDL pthis, mfxMemId mid, void *tex, mfxU64 lock_key,
                         mfxU64 *next_key) {
  UNUSED_PARAMETER(lock_key);
  UNUSED_PARAMETER(next_key);

  profile_start("copy_tex");

  mfxSession *session = (mfxSession *)pthis;
  VADisplay display;
  mfxStatus sts = MFXVideoCORE_GetHandle(*session, DEVICE_MGR_TYPE, &display);
  MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

  struct encoder_texture *ptex = (struct encoder_texture *)tex;
  struct surface_info *surf = (struct surface_info *)mid;

  obs_enter_graphics();
  // Error reporting for copy is poor, if earlier surface handling is
  // broken this will do bad things.
  gs_copy_texture(surf->tex_y, ptex->tex[0]);
  gs_copy_texture(surf->tex_uv, ptex->tex[1]);
  obs_leave_graphics();

  profile_end("copy_tex");
  return MFX_ERR_NONE;
}

struct get_drm_device_params {
  const char **out_path;
  uint32_t idx;
};

bool get_drm_device(void *param, const char *node, uint32_t idx) {
  struct get_drm_device_params *p = (struct get_drm_device_params *)param;
  if (idx == p->idx) {
    *p->out_path = node;
    return false;
  }
  return true;
}

struct linux_data {
  int fd;
  VADisplay vaDisplay;
};

// Release per session resources.
void ReleaseSessionData(void *data) {
  struct linux_data *d = (struct linux_data *)data;
  if (d) {
    vaTerminate(d->vaDisplay);
    close(d->fd);
    bfree(d);
  }
}

extern "C" void util_cpuid(int cpuinfo[4], int level) {
  __get_cpuid(level, (unsigned int *)&cpuinfo[0], (unsigned int *)&cpuinfo[1],
              (unsigned int *)&cpuinfo[2], (unsigned int *)&cpuinfo[3]);
}

struct vaapi_device {
  int fd;
  VADisplay display;
  const char *driver;
};

static void vaapi_open(const char *device_path, struct vaapi_device *device) {
  int fd = open(device_path, O_RDWR);
  if (fd < 0) {
    return;
  }

  VADisplay display = vaGetDisplayDRM(fd);
  if (!display) {
    close(fd);
    return;
  }

  // VA-API is noisy by default.
  vaSetInfoCallback(display, nullptr, nullptr);
  vaSetErrorCallback(display, nullptr, nullptr);

  int major;
  int minor;
  if (vaInitialize(display, &major, &minor) != VA_STATUS_SUCCESS) {
    vaTerminate(display);
    close(fd);
    return;
  }

  const char *driver = vaQueryVendorString(display);
  if (strstr(driver, "Intel i965 driver") != nullptr) {
    blog(LOG_WARNING,
         "Legacy intel-vaapi-driver detected, incompatible with QSV");
    vaTerminate(display);
    close(fd);
    return;
  }

  device->fd = fd;
  device->display = display;
  device->driver = driver;
}

static void vaapi_close(struct vaapi_device *device) {
  vaTerminate(device->display);
  close(device->fd);
}

static uint32_t vaapi_check_support(VADisplay display, VAProfile profile,
                                    VAEntrypoint entrypoint) {
  bool ret = false;
  VAConfigAttrib attrib[1]{};
  attrib->type = VAConfigAttribRateControl;

  VAStatus va_status =
      vaGetConfigAttributes(display, profile, entrypoint, attrib, 1);

  uint32_t rc = 0;
  switch (va_status) {
  case VA_STATUS_SUCCESS:
    rc = attrib->value;
    break;
  case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
  case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
  default:
    break;
  }

  return (rc & VA_RC_CBR || rc & VA_RC_CQP || rc & VA_RC_VBR);
}

static bool vaapi_supports_h264(VADisplay display) {
  bool ret = false;
  ret |= vaapi_check_support(display, VAProfileH264ConstrainedBaseline,
                             VAEntrypointEncSlice);
  ret |= vaapi_check_support(display, VAProfileH264Main, VAEntrypointEncSlice);
  ret |= vaapi_check_support(display, VAProfileH264High, VAEntrypointEncSlice);

  if (!ret) {
    ret |= vaapi_check_support(display, VAProfileH264ConstrainedBaseline,
                               VAEntrypointEncSliceLP);
    ret |=
        vaapi_check_support(display, VAProfileH264Main, VAEntrypointEncSliceLP);
    ret |=
        vaapi_check_support(display, VAProfileH264High, VAEntrypointEncSliceLP);
  }

  return ret;
}

static bool vaapi_supports_av1(VADisplay display) {
  bool ret = false;
  // Are there any devices with non-LowPower entrypoints?
  ret |=
      vaapi_check_support(display, VAProfileAV1Profile0, VAEntrypointEncSlice);
  ret |= vaapi_check_support(display, VAProfileAV1Profile0,
                             VAEntrypointEncSliceLP);
  return ret;
}

static bool vaapi_supports_hevc(VADisplay display) {
  bool ret = false;
  ret |= vaapi_check_support(display, VAProfileHEVCMain, VAEntrypointEncSlice);
  ret |=
      vaapi_check_support(display, VAProfileHEVCMain, VAEntrypointEncSliceLP);
  return ret;
}

static bool vaapi_supports_vp9(VADisplay display) {
  bool ret = false;
  ret |= vaapi_check_support(display, VAProfileVP9Main, VAEntrypointEncSlice);
  ret |= vaapi_check_support(display, VAProfileVP9Main, VAEntrypointEncSliceLP);
  return ret;
}

bool check_adapter(void *param, const char *node, uint32_t idx) {
  struct vaapi_device device = {0};
  struct adapter_info *adapters = (struct adapter_info *)param;

  vaapi_open(node, &device);
  if (!device.display)
    return true;

  struct adapter_info *adapter = &adapters[idx];
  adapter->is_intel = strstr(device.driver, "Intel") != nullptr;
  // This is currently only used for LowPower coding which is busted on VA-API
  // anyway.
  adapter->is_dgpu = false;
  adapter->supports_av1 = vaapi_supports_av1(device.display);
  adapter->supports_hevc = vaapi_supports_hevc(device.display);
  adapter->supports_vp9 = vaapi_supports_vp9(device.display);

  if (adapter->is_intel && default_h264_device == nullptr)
    default_h264_device = strdup(node);

  if (adapter->is_intel && adapter->supports_av1 &&
      default_av1_device == nullptr)
    default_av1_device = strdup(node);

  if (adapter->is_intel && adapter->supports_hevc &&
      default_hevc_device == nullptr)
    default_hevc_device = strdup(node);

  if (adapter->is_intel && adapter->supports_vp9 &&
      default_vp9_device == nullptr)
    default_vp9_device = strdup(full_path.array);

  vaapi_close(&device);
  return true;
}

void check_adapters(struct adapter_info *adapters, size_t *adapter_count) {
  obs_enter_graphics();
  uint32_t gs_count = gs_get_adapter_count();
  if (*adapter_count < gs_count) {
    blog(LOG_WARNING, "Too many video adapters: %ld < %d", *adapter_count,
         gs_count);
    obs_leave_graphics();
    return;
  }
  gs_enum_adapters(check_adapter, adapters);
  obs_leave_graphics();
}
