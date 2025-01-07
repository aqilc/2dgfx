#include <time.h>
#include <stdio.h>

#define UDP_IMPLEMENTATION
#include "../udp.h"

// #define MA_DEBUG_OUTPUT
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "util.c"

// Windows smh
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef _WIN32
#define Sleep sleep
#endif

ma_encoder encoder;
ma_encoder capture_encoder;
struct voice_packet {
  uint16_t id; // Session ID for client, Voice ID for Server
  uint16_t len;
  uint32_t seq;
  uint8_t data[];
};

struct voice {
  uint16_t id;
  uint16_t session_id;
  uint32_t packet_seq;
  uint16_t packet_len;
  uint16_t packet_played_len;
  uint32_t packet_allocated;
  char* packet_data;
  union {
    udp_addr* addr;
    ma_mutex writing_packet;
  };
}* voices = NULL;
int voices_len = 0;
uint16_t session_id;

void capture_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
  static uint32_t seq = 0;
  int data_len = frameCount * ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels);

  struct voice_packet* pack = alloca(sizeof(struct voice_packet) + data_len);
  pack->id = session_id;
  pack->len = data_len;
  pack->seq = ++seq;
  memcpy(pack->data, pInput, data_len);
  if(seq >= 2 && seq <= 10) {
    printf("Sending %d bytes of data with the first few samples of audio being: %x, %x, %x, %x\n", data_len, ((short*)pInput)[0], ((short*)pInput)[1], ((short*)pInput)[2], ((short*)pInput)[3]);
  }

  ma_encoder_write_pcm_frames(&capture_encoder, pInput, frameCount, NULL);

  udp_send((udp_conn*)pDevice->pUserData, (void*)pack, data_len + sizeof(struct voice_packet));
}

void playback_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
  
  // Mix all voices
  for(int i = 0; i < voices_len; i++) {
    struct voice* voice = voices + i;

    if(voice->packet_played_len < voice->packet_len) {
      ma_mutex_lock(&voice->writing_packet);
      int len = min(voice->packet_len - voice->packet_played_len, frameCount * sizeof(short)) / sizeof(short);

      for(int j = 0; j < len; j ++)
        ((short*) pOutput)[j] = (short) min(max((int) ((short*) pOutput)[j] + (int) ((short*) ((char*)voice->packet_data + voice->packet_played_len))[j], -32768), 32767);
      
      voice->packet_played_len += len * sizeof(short);
      ma_mutex_unlock(&voice->writing_packet);
    }
  }
}

struct voice* push_voice_serv(udp_addr* addr, uint16_t session_id) {
  voices = realloc(voices, sizeof(struct voice) * (++voices_len));
  voices[voices_len - 1] = (struct voice) {
    .addr = memcpy(malloc(sizeof(udp_addr)), addr, sizeof(udp_addr)),
    .id = voices_len, .session_id = session_id
  };
  printf("Client #%d(@%d) connected from %s.\n", voices_len, session_id, udp_addr_str(addr, (char[256]) {}, 256));
  return voices + voices_len - 1;
}

struct voice* push_voice_cli(int id) {
  voices = realloc(voices, sizeof(struct voice) * (++voices_len));
  voices[voices_len - 1] = (struct voice) { .id = id, .packet_data = malloc(1024), .packet_allocated = 1024 };
  ma_mutex_init(&voices[voices_len - 1].writing_packet);
  printf("Peer with ID %d connected.\n", id);

  ma_encoder_config conf_encoder = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 1, 48000);
  char str[] = { 'o', 'u', 't', 'p', 'u', 't', '0' + id, '.', 'w', 'a', 'v', '\0' };
  if(ma_encoder_init_file(str, &conf_encoder, &encoder)) {
    printf("Failed to initialize client encoder #%d.\n", id);
    exit(1);
  }
  return voices + voices_len - 1;
}

bool is_server = false;
int main(const int argc, const char** argv) {
  is_server = argc == 1;
  
  ma_device capture_device;
  ma_device playback_device;
  udp_conn* conn;
  if(is_server) conn = udp_serve(30000);
  else conn = udp_connect(udp_resolve_host(argv[1], "30000", true, &(udp_addr) {}), false);
  
  if(conn->error) {
    printf("Error establishing a connection: %s\n", udp_error_str(conn->error));
    return 1;
  }


  if(!is_server) {
    printf("Connected to %s.\n", udp_addr_str(&conn->from, (char[256]) {}, 256));
    srand(time(NULL));
    session_id = rand();
    
    ma_device_config conf_capture    = ma_device_config_init(ma_device_type_capture);
    ma_device_config conf_playback   = ma_device_config_init(ma_device_type_playback);
    conf_playback.playback.format    = conf_capture.capture.format     = ma_format_s16;
    conf_playback.playback.channels  = conf_capture.capture.channels   = 1;
    conf_playback.sampleRate         = conf_capture.sampleRate         = 48000;
    conf_playback.periodSizeInFrames = conf_capture.periodSizeInFrames = 512;
    conf_capture.dataCallback        = capture_callback;
    conf_capture.pUserData           = conn;
    conf_playback.dataCallback       = playback_callback;
    conf_playback.pUserData          = NULL;

    ma_encoder_config conf_encoder = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 1, 48000);
    char str[] = { 'i', 'n', 'p', 'u', 't', '0' + (session_id / 1000) % 10, '0' + (session_id / 100) % 10, '0' + (session_id / 10) % 10, '0' + session_id % 10, '.', 'w', 'a', 'v', '\0' };
    if(ma_encoder_init_file(str, &conf_encoder, &capture_encoder)) {
      printf("Failed to initialize capture encoder.\n");
      exit(1);
    }
    if(ma_device_init(NULL, &conf_capture, &capture_device) || ma_device_init(NULL, &conf_playback, &playback_device) ||
       ma_device_start(&capture_device) || ma_device_start(&playback_device)) {
      printf("Failed to connect to capture and/or playback devices.\n");
      return 1;
    }
  } else printf("Listening to %s.\n", udp_addr_str(&conn->from, (char[256]) {}, 256));
  
  puts("Press enter to quit.");
  voices = calloc(1, sizeof(struct voice));

  while(true) {
    while(!(is_server ? udp_recv_from : udp_recv)(conn)) {
      if(conn->error) {
        printf("Error: %s\n", udp_error_str(conn->error));
        goto done;
      }
      if(_kbhit() && getchar()) goto done;
      // Sleep(1);
    }
    
    struct voice_packet* packet = (struct voice_packet*) conn->data;    
    struct voice* voice = NULL;
    
    if(is_server) {
      for(int i = 0; i < voices_len; i++) {
        if(udp_addr_same(voices[i].addr, &conn->from)) {
          voice = voices + i;

          // Reset the session if the client is reconnecting
          if(packet->id != voice->session_id) {
            voice->session_id = packet->id;
            voice->packet_seq = 0;
            printf("Client #%d reconnected.\n", voice->id);
          }
          break;
        }
      }

      if(!voice) voice = push_voice_serv(&conn->from, packet->id);
      if(packet->seq <= voice->packet_seq) continue;

      packet->id = voice->id;
      for(int i = 0; i < voices_len; i++)
        if(voice->id != voices[i].id) udp_send_to(conn, voices[i].addr, packet, conn->data_len); //, printf("s");
      
    } else {
      for(int i = 0; i < voices_len; i++)
        if(voices[i].id == packet->id) { voice = voices + i; break; }
 
      if(!voice) voice = push_voice_cli(packet->id);
      if(packet->seq <= voice->packet_seq) continue;
      
      if(packet->seq >= 2 && packet->seq <= 10) {
        printf("Recieving %d(total %d) bytes of data with the first few samples of audio being: %x, %x, %x, %x\n", packet->len, (int)conn->data_len, ((short*)packet->data)[0], ((short*)packet->data)[1], ((short*)packet->data)[2], ((short*)packet->data)[3]);
      }
      ma_encoder_write_pcm_frames(&encoder, packet->data, packet->len / sizeof(short) / 1, NULL);
      ma_mutex_lock(&voice->writing_packet);
      
      // If there was unplayed audio, move it to the beginning of the buffer so it can still be played
      uint32_t buf_start = 0;
      if(voice->packet_played_len < voice->packet_len) {
        buf_start = min(voice->packet_len - voice->packet_played_len, 20480); // Limit the buffer size to ~20KB

        if(voice->packet_played_len > 0) // Don't need to move anything if there was nothing played lol
          memmove(voice->packet_data, voice->packet_data + voice->packet_played_len, buf_start);
      }
      
      voice->packet_len = packet->len + buf_start;
      voice->packet_played_len = 0;
      if(voice->packet_allocated < voice->packet_len) {
        voice->packet_allocated = voice->packet_len;
        voice->packet_data = realloc(voice->packet_data, voice->packet_len);
      }
      memcpy(voice->packet_data + buf_start, packet->data, packet->len);
      ma_mutex_unlock(&voice->writing_packet);
    }

    voice->packet_seq = packet->seq;
  }

done:
  if(!is_server) {
    ma_device_uninit(&capture_device);
    ma_device_uninit(&playback_device);
    ma_encoder_uninit(&encoder);
  }
  udp_close_n_free(conn);
  return 0;
}
