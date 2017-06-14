#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <stdio.h>
#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif
#include "NDS.h"
#include "GPU.h"
#include "SPU.h"
#include "libretro.h"

#define VIDEO_WIDTH 256
#define VIDEO_HEIGHT 384
#define VIDEO_PIXELS VIDEO_WIDTH * VIDEO_HEIGHT

#ifdef HAVE_THREADS
/*  Code taken from http://greenteapress.com/semaphores/semaphore.c
 *  and changed to use libretro-common's mutexes and conditions.
 */

#include <stdlib.h>

#include <rthreads/rthreads.h>

typedef struct ssem ssem_t;

struct ssem
{
   int value;
   int wakeups;
   slock_t *mutex;
   scond_t *cond;
};

ssem_t *ssem_new(int value)
{
   ssem_t *semaphore = (ssem_t*)calloc(1, sizeof(*semaphore));

   if (!semaphore)
      goto error;
   
   semaphore->value   = value;
   semaphore->wakeups = 0;
   semaphore->mutex   = slock_new();

   if (!semaphore->mutex)
      goto error;

   semaphore->cond = scond_new();

   if (!semaphore->cond)
      goto error;

   return semaphore;

error:
   if (semaphore->mutex)
      slock_free(semaphore->mutex);
   semaphore->mutex = NULL;
   if (semaphore)
      free((void*)semaphore);
   return NULL;
}

void ssem_free(ssem_t *semaphore)
{
   if (!semaphore)
      return;

   scond_free(semaphore->cond);
   slock_free(semaphore->mutex);
   free((void*)semaphore);
}

void ssem_wait(ssem_t *semaphore)
{
   if (!semaphore)
      return;

   slock_lock(semaphore->mutex);
   semaphore->value--;

   if (semaphore->value < 0)
   {
      do
      {
         scond_wait(semaphore->cond, semaphore->mutex);
      }while (semaphore->wakeups < 1);

      semaphore->wakeups--;
   }

   slock_unlock(semaphore->mutex);
}

void ssem_signal(ssem_t *semaphore)
{
   if (!semaphore)
      return;

   slock_lock(semaphore->mutex);
   semaphore->value++;

   if (semaphore->value <= 0)
   {
      semaphore->wakeups++;
      scond_signal(semaphore->cond);
   }

   slock_unlock(semaphore->mutex);
}
#endif

static uint8_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
char retro_base_directory[4096];
char retro_game_path[4096];
bool retro_firmware_status;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}


static retro_environment_t environ_cb;

void retro_init(void)
{
   frame_buf = (uint8_t*)malloc(VIDEO_PIXELS * sizeof(uint32_t));
   const char *dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
   {
      snprintf(retro_base_directory, sizeof(retro_base_directory), "%s", dir);
   }
}

void retro_deinit(void)
{
   free(frame_buf);
   frame_buf = NULL;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "melonDS";
   info->library_version  = "0.3";
   info->need_fullpath    = true;
   info->valid_extensions = "nds";
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

namespace Platform
{

void Semaphore_Reset(void *sema)
{
   /* TODO/FIXME */
}

void Semaphore_Post(void *sema)
{
#if 0
#ifdef HAVE_THREADS
   ssem_signal((ssem_t*)sema);
#endif
#endif
}

void Semaphore_Wait(void *sema)
{
#if 0
#ifdef HAVE_THREADS
   ssem_wait((ssem_t*)sema);
#endif
#endif
}

void Semaphore_Free(void *sema)
{
#if 0
#ifdef HAVE_THREADS
   ssem_t *sem = (ssem_t*)sema;
   if (sem)
      ssem_free(sem);
#endif
#endif
}

void *Semaphore_Create()
{
#if 0
#ifdef HAVE_THREADS
   ssem_t *sem = ssem_new(0);
   if (sem)
      return sem;
#endif
#endif
   return NULL;
}

void Thread_Free(void *thread)
{
   /* TODO/FIXME */
}

void *Thread_Create(void (*func)())
{
   /* TODO/FIXME */
   return NULL;
}

void Thread_Wait(void *thread)
{
   /* TODO/FIXME */
}

int MP_SendPacket(u8 *data, int len)
{
   /* TODO/FIXME */
   return 0;
}

int MP_RecvPacket(u8 *data, bool block)
{
   /* TODO/FIXME */
   return 0;
}

void MP_DeInit()
{
   /* TODO/FIXME */
}

bool MP_Init()
{
   /* TODO/FIXME */
   return true;
}

};

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing.fps            = 32.0f * 1024.0f * 1024.0f / 560190.0f;
   info->timing.sample_rate    = 32.0f * 1024.0f;
   info->geometry.base_width   = VIDEO_WIDTH;
   info->geometry.base_height  = VIDEO_HEIGHT;
   info->geometry.max_width    = VIDEO_WIDTH;
   info->geometry.max_height   = VIDEO_HEIGHT;
   info->geometry.aspect_ratio = 0.0f;
}

static struct retro_rumble_interface rumble;

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;
   else
      log_cb = fallback_log;

   static const struct retro_controller_description controllers[] = {
      { "Nintendo DS", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0) },
   };

   static const struct retro_controller_info ports[] = {
      { controllers, 1 },
      { NULL, 0 },
   };

   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

static unsigned x_coord;
static unsigned y_coord;
static unsigned phase;
static int mouse_rel_x;
static int mouse_rel_y;

void retro_reset(void)
{
   x_coord = 0;
   y_coord = 0;
}

static void update_input(void)
{
   uint16_t keys = 0;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A)) << 0;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B)) << 1;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT)) << 2;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START)) << 3;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)) << 4;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)) << 5;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP)) << 6;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN)) << 7;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R)) << 8;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L)) << 9;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X)) << 10;
   keys |= (!!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y)) << 11;

   for (uint8_t i = 0; i < 12; i++) {
      bool key = !!((keys >> i) & 1);
      uint8_t nds_key = i > 9 ? i + 6 : i;

      
      if (key) {
         NDS::PressKey(nds_key);
      } else {
         NDS::ReleaseKey(nds_key);
      }
   }
}


static void check_variables(void)
{

}

static void audio_callback(void)
{
   static int16_t buffer[0x1000];
   u32 avail = SPU::Available();
   if(avail > sizeof(buffer) / (2 * sizeof(int16_t)))
      avail = sizeof(buffer) / (2 * sizeof(int16_t));

   SPU::ReadOutput(buffer, avail);
   audio_cb(buffer, avail);
}

void retro_run(void)
{
   unsigned i;
   update_input();
   NDS::RunFrame();
   for (i = 0; i < VIDEO_PIXELS * sizeof(uint32_t); i += sizeof(uint32_t))
   {
      uint8_t *pixel = (uint8_t*)GPU::Framebuffer + i;
      /* swap red and blue */
      frame_buf[i + 0] = pixel[2];
      frame_buf[i + 1] = pixel[1];
      frame_buf[i + 2] = pixel[0];
      frame_buf[i + 3] = pixel[3];
   }
   video_cb(frame_buf, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_WIDTH * sizeof(uint32_t));
   audio_callback();

   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();
}

bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
      return false;
   }

   snprintf(retro_game_path, sizeof(retro_game_path), "%s", info->path);
   NDS::Init();
   NDS::LoadROM(info->path, true);

   check_variables();

   (void)info;
   if (!retro_firmware_status)
      return false;

   return true;
}

void retro_unload_game(void)
{

}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   return false;
}

size_t retro_serialize_size(void)
{
   return false;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}
