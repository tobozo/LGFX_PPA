/*\
 *
 * PPA operation utils for LovyanGFX/M5GFX
 *
 * An incomplete experiment brought to you by tobozo, copyleft (c+) Aug. 2025
 *
\*/
#pragma once


#include <sdkconfig.h>
#if defined CONFIG_IDF_TARGET_ESP32P4 || defined CONFIG_IDF_TARGET_ESP32S31
  #define LGFX_PPA_SUPPORTED
  #define LGFX_PPA_ALIGN_SIZE CONFIG_CACHE_L1_CACHE_LINE_SIZE
#endif


#if !defined LGFX_PPA_SUPPORTED

  #error "PPA Operations are only available on ESP32P4 or ESP32S31"

#else

  #if __has_include(<soc/soc_caps.h>)
    #include <soc/soc_caps.h>
    #if SOC_MIPI_DSI_SUPPORTED
      #define LGFX_PPA_HAS_PANEL_DSI // TODO: get rid of this macro
    #endif
  #endif


  #if __has_include(<M5GFX.h>) || __has_include(<M5Unified.hpp>)

    #include <M5GFX.h>
    #include <lgfx/v1/panel/Panel_FrameBufferBase.hpp>
    #if defined LGFX_PPA_HAS_PANEL_DSI // TODO: get rid of this block
      #include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>
      using m5gfx::Panel_DSI;
    #endif

  #elif __has_include(<LovyanGFX.hpp>)

    #include <LovyanGFX.hpp>
    #include <lgfx/v1/panel/Panel_FrameBufferBase.hpp>
    #if defined LGFX_PPA_HAS_PANEL_DSI // TODO: get rid of this block
      #include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>
      using lgfx::Panel_DSI;
    #endif

  #else

    #error "Please include <M5GFX.h>, <M5Unified.hpp> or <LovyanGFX.hpp> before including this file"

  #endif


  // import esp_driver_ppa
  extern "C"
  {
    #include "driver/ppa.h"
    #include "esp_heap_caps.h"
    #include "esp_cache.h"
    #include "esp_private/esp_cache_private.h"
    #include "esp_log.h"
  }


  namespace lgfx
  {

    // debug tags for all ppa utils
    static constexpr const char* PPA_SPRITE_TAG = "PPA_Sprite";
    static constexpr const char* PPA_TAG        = "PPABase";
    static constexpr const char* SRM_TAG        = "PPASrm";
    static constexpr const char* BLEND_TAG      = "PPABlend";
    static constexpr const char* FILL_TAG       = "PPAFill";

    class PPA_Sprite;
    class PPABase;
    class PPAFill;
    class PPABlend;
    class PPASrm;

    // this should be part of lgfx
    struct clipRect_t { int32_t x; int32_t y; int32_t w; int32_t h; };

    // Callback: LGFX_Device buffer provider
    // Only Panel_DSI and LGFX_Sprite can provide their buffer so far, however with a few modifications,
    // and provided the buffer is properly aligned during its allocation, Panel_AMOLED, Panel_RGB and a
    // few other Panel_Framebuffer* based panels could easily implement it.
    typedef std::function<bool(LGFX_Device*gfx, void* &dst_buffer)> dev_buffer_provider_t;

    // debug helper to compensate for typeid() being disabled by compiler
    template <typename T> const char *TYPE_NAME()
    {
      #ifdef _MSC_VER
        return __FUNCSIG__;
      #else
        return __PRETTY_FUNCTION__;
      #endif
    }

    // memory helper for ppa operations
    void* heap_alloc_ppa(size_t length, size_t*size=nullptr);

    // buffer size helper for ppa operations
    inline uint32_t lgfx_ppa_align_up(uint32_t bufsize) { return ((bufsize + LGFX_PPA_ALIGN_SIZE - 1) / LGFX_PPA_ALIGN_SIZE) * LGFX_PPA_ALIGN_SIZE; }

    // on_trans_done() callbacks
    bool lgfx_ppa_cb_sem_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data);
    bool lgfx_ppa_cb_bool_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data);

    // a little debug helper
    const char* ppa_operation_type_to_string( ppa_operation_t oper_type );

    // lgfx to ppa color modes
    ppa_fill_color_mode_t ppa_fill_color_mode(uint8_t lgfx_color_bit_depth);
    ppa_blend_color_mode_t ppa_blend_color_mode(uint8_t lgfx_color_bit_depth);
    ppa_srm_color_mode_t ppa_srm_color_mode(uint8_t lgfx_color_bit_depth);

    // float to axis lossy angle rotation
    ppa_srm_rotation_angle_t ppa_srm_get_rotation_from_angle(float angle);

    // axis to float angle rotation
    float ppa_srm_get_angle_from_rotation(ppa_srm_rotation_angle_t rotation);

    // lgfx to ppa rgb888 color converter
    template<typename T>
    color_pixel_rgb888_data_t ppa_color_convert_rgb888(T c)
    {
      rgb888_t ct = c; // NOTE: implicit conversion
      return {.b=ct.B8(), .g=ct.G8(), .r=ct.R8() };
    }

    // lgfx to ppa argb8888 color converter
    template<typename T>
    color_pixel_argb8888_data_t ppa_color_convert_argb8888(T c)
    {
      argb8888_t ct = c; // NOTE: implicit conversion
      return {.b=ct.B8(), .g=ct.G8(), .r=ct.R8(), .a=ct.A8() };
    }

    // LGFX_Device/LGFX_Sprite bitDepth callback provider for lgfx_ppa_get*_buffer functions
    template <typename GFX> // GFX = LGFX_Device/LGFX_Sprite family
    bool lgfx_ppa_get_bit_depth(LGFX_Device*gfx, uint8_t &bitDepth)
    {
      uint8_t _bitDepth = gfx->getColorDepth() & 0xff;
      if( _bitDepth < 16 ) {
        ESP_LOGE(PPA_TAG, "Unsupported bit depth: %d for type %s", _bitDepth, TYPE_NAME<GFX>() );
        return false;
      }
      bitDepth = _bitDepth;
      return true;
    }

    // Buffer providers

    // LGFX_Device buffer callback provider via LGFX_Device::getPanel()::config_detail().buffer e.g. Panel_DSI
    template <typename PanelConfigFB>
    bool lgfx_ppa_get_panel_config_buffer(LGFX_Device*gfx, void* &dst_buffer)
    {
      static_assert( std::is_convertible<PanelConfigFB,Panel_FrameBufferBase>::value, "PanelConfigFB is not derived from Panel_FrameBufferBase" );
      dst_buffer = ((PanelConfigFB*)gfx->getPanel())->config_detail().buffer;
      return true;
    }

    // LGFX_Device buffer callback provider with custom panel via LGFX_Device::getPanel()::getBuffer()
    template <typename PanelFB>
    bool lgfx_ppa_get_panel_buffer(LGFX_Device*gfx, void* &dst_buffer)
    {
      static_assert( std::is_convertible<PanelFB,Panel_Device>::value, "PanelFB is not derived from Panel_Device" );
      dst_buffer = ((PanelFB*)gfx->getPanel())->getBuffer();
      return true;
    }

    // PPA_Sprite/LGFX_Sprite buffer callback provider via ::getBuffer()
    template <typename FB>
    bool lgfx_ppa_get_buffer(LGFX_Device*gfx, void* &dst_buffer)
    {
      static_assert( std::is_convertible<PanelFB,LGFX_Sprite>::value, "FB is not derived from LGFX_Sprite" );
      dst_buffer = ((FB*)gfx)->getBuffer();
      return true;
    }

    // alias the LGFX_Sprite specialised callback provider
    inline dev_buffer_provider_t lgfx_ppa_get_sprite_buffer = lgfx_ppa_get_buffer<LGFX_Sprite>;


    #if defined LGFX_PPA_HAS_PANEL_DSI

      // LGFX_PPA needs to read/write the panel buffer, which must be properly aligned for ppa operations.
      // See typedef dev_buffer_provider_t, the callback is set in PPABase constructor.
      // NOTE: Panel_DSI is currently used as the default callback.

      #if !defined LGFX_PPA_DEVICE_BUFFER_PROVIDER
        inline dev_buffer_provider_t lgfx_ppa_get_panel_dsi_buffer = lgfx_ppa_get_panel_config_buffer<Panel_DSI>;
        #define LGFX_PPA_DEVICE_BUFFER_PROVIDER lgfx_ppa_get_panel_dsi_buffer
      #endif

    #endif


    #if !defined LGFX_PPA_DEVICE_BUFFER_PROVIDER
      #define LGFX_PPA_DEVICE_BUFFER_PROVIDER nullptr // lgfx_ppa_get_sprite_buffer
    #endif


    // ---------------------------------------------------------------------------------------------


    // Same as LGFX_Sprite but with aligned memory for ppa operations, and restricted to 16 bits colors
    class PPA_Sprite : public LGFX_Sprite
    {
    public:
      PPA_Sprite(LovyanGFX* parent) : LGFX_Sprite(parent)
      {
        _panel = &_panel_sprite;
        setColorDepth(16);
        setPsram(true);
      }
      PPA_Sprite() : PPA_Sprite(nullptr) {}
      void* createSprite(int32_t w, int32_t h);
    };

    static SemaphoreHandle_t ppa_semaphore = nullptr;


    // ---------------------------------------------------------------------------------------------



    class PPABase
    {
    public:

      ~PPABase();

      void setTransferDone(bool val);
      SemaphoreHandle_t getSemaphore();
      bool available();

      template <typename GFX>
      PPABase(GFX* out, ppa_operation_t oper_type, bool async = true, bool use_semaphore = false, dev_buffer_provider_t getbuf = nullptr)
      : ppa_client_config({.oper_type=oper_type, .max_pending_trans_num=1, .data_burst_length=PPA_DATA_BURST_LENGTH_128}),
        async(async), use_semaphore(use_semaphore),
        output_w(out->width()), output_h(out->height()), outputGFX((LGFX_Device*)out),
        getDeviceBuffer(getbuf), is_panel( std::is_convertible<GFX, LGFX_Device>::value )
      {
        enabled = false;

        if(!getDeviceBuffer) {
          ESP_LOGV(PPA_TAG, "Base constructor called without cb for %s", TYPE_NAME<GFX>() );
        }

        if( output_w==0 || output_h==0 ) { // TODO: better check, ppa_operations have more constraints on output dimensions
          ESP_LOGE(PPA_TAG, "Bad output dimensions: w = %d, h = %d", output_w, output_h);
          return;
        }

        if(!async && use_semaphore) {
          ESP_LOGW(PPA_TAG, "use_semaphore=true but async=false, async will be enabled anyway");
          async = true;
        }

        if( ppa_semaphore == NULL )
          ppa_semaphore = xSemaphoreCreateBinary();

        // use either a semaphore or a volatile boolean to handle (a)synchronicity
        ppa_event_cb.on_trans_done = (async && use_semaphore)? lgfx_ppa_cb_sem_func : lgfx_ppa_cb_bool_func;

        if( ESP_OK != ppa_register_client(&ppa_client_config, &ppa_client_handle) ) {
          ESP_LOGE(PPA_TAG, "Failed to ppa_register_client");
          return;
        }

        if( ESP_OK != ppa_client_register_event_callbacks(ppa_client_handle, &ppa_event_cb) ) {
          ESP_LOGE(PPA_TAG, "Failed to ppa_client_register_event_callbacks");
          return;
        }

        ppa_out_pic_blk_config_t out_cfg;
        if( ! config_block_out<GFX>(&out_cfg) ) {
          ESP_LOGE(PPA_TAG, "Failed to set config block out for %s", TYPE_NAME<GFX>() );
          return;
        }

        base_inited = true;
      }


      // Attach a callback to retrieve the LGFX_Device output buffer.
      // YMMV depending on the panel.
      inline void setDeviceBufferProvider(dev_buffer_provider_t getbuf)
      {
        assert(getbuf);
        getDeviceBuffer = getbuf;
      }

      // Attach a callback to retrieve the sprite buffer.
      // Optional, a default callback is already set for LGFX_Sprite.
      inline void setSpriteBufferProvider(dev_buffer_provider_t getbuf)
      {
        assert(getbuf);
        getSpriteBuffer = getbuf;
      }


      // execute ppa operation
      template <typename T>
      bool exec(T *cfg)
      {
        if(!ready() || !cfg)
          return false;
        esp_err_t ret = ESP_FAIL;

        //LGFX_PPA_RESET_CACHE(output_buffer, output_w*output_h*output_bytes_per_pixel);

        switch(ppa_client_config.oper_type) {
          case PPA_OPERATION_SRM:   ret = ppa_do_scale_rotate_mirror(ppa_client_handle, (ppa_srm_oper_config_t*)cfg); break;
          case PPA_OPERATION_BLEND: ret = ppa_do_blend(ppa_client_handle, (ppa_blend_oper_config_t*)cfg); break;
          case PPA_OPERATION_FILL:  ret = ppa_do_fill(ppa_client_handle, (ppa_fill_oper_config_t*)cfg); break;
          default: ESP_LOGE(PPA_TAG, "Unimplemented PPA operation %d", ppa_client_config.oper_type);
        }

        if( ret!=ESP_OK ) { // callback failed, reset transfer
          setTransferDone(true);
          return false;
        }
        return true;
      }


    protected:

      // ppa operations config
      ppa_client_config_t ppa_client_config;
      ppa_client_handle_t ppa_client_handle = nullptr;
      ppa_event_callbacks_t ppa_event_cb;

      // semaphore
      volatile bool ppa_transfer_done = true;

      // init/available() state machine
      bool base_inited  = false;
      bool enabled = true;

      // (a)synchronicity behaviour
      const bool async;
      const bool use_semaphore;

      // output properties

      const uint32_t output_w;
      const uint32_t output_h;

      uint8_t output_bytes_per_pixel;

      void* output_buffer = nullptr;
      uint32_t output_buffer_size;

      LGFX_Device* outputGFX;

      // device buffer provider
      dev_buffer_provider_t getDeviceBuffer = nullptr;
      dev_buffer_provider_t getSpriteBuffer = lgfx_ppa_get_sprite_buffer;

      const bool is_panel; // affects byte swap

      bool ready();

      bool config_block_out(ppa_out_pic_blk_config_t *cfg, void*buffer, uint32_t buffer_size, clipRect_t clipRect, uint8_t bitDepth);
      bool config_block_in(ppa_in_pic_blk_config_t* cfg, void*buffer, uint32_t w, uint32_t h, clipRect_t clipRect, uint8_t bitDepth);

      template <typename GFX>
      bool config_block_in(ppa_in_pic_blk_config_t* cfg, GFX* gfx)
      {
        if(!gfx || !cfg)
          return false;
        void *buf;
        uint8_t bitDepth = 16;

        if( !get_buffer(gfx, buf, bitDepth) ) {
          ESP_LOGE(PPA_TAG, "Failed to get input buffer for %s", TYPE_NAME<GFX>() );
          return false;
        }

        clipRect_t clipRect = {0,0,0,0};
        gfx->getClipRect(&clipRect.x, &clipRect.y, &clipRect.w, &clipRect.h);

        return config_block_in(cfg, buf, gfx->width(), gfx->height(), clipRect, bitDepth);
      }

      template <typename GFX>
      bool config_block_out(ppa_out_pic_blk_config_t *cfg)
      {
        if(!cfg) {
          ESP_LOGE(PPA_TAG, "No cfg<%s> provided", TYPE_NAME<GFX>() );
          return false;
        }
        uint8_t bitDepth = 16;

        if( !get_buffer((GFX*)outputGFX, output_buffer, bitDepth) ) {
          ESP_LOGE(PPA_TAG, "Failed to get output buffer for %s", TYPE_NAME<GFX>() );
          return false;
        }

        if( !output_buffer ) {
          ESP_LOGE(PPA_TAG, "invalid output buffer for %s", TYPE_NAME<GFX>() );
          return false;
        }

        output_bytes_per_pixel = bitDepth/8;

        clipRect_t clipRect = {0,0,0,0};
        outputGFX->getClipRect(&clipRect.x, &clipRect.y, &clipRect.w, &clipRect.h);

        output_buffer_size = lgfx_ppa_align_up( clipRect.w * clipRect.h * output_bytes_per_pixel);

        return config_block_out(cfg, output_buffer, clipRect.w*clipRect.h*output_bytes_per_pixel, clipRect, bitDepth );
      }

      // get device or sprite buffer, for reading or writing
      template <typename GFX>
      bool get_buffer(GFX* gfx, void* &dst_buffer, uint8_t &bitDepth)
      {
        if( !gfx ) {
          ESP_LOGE(PPA_TAG, "No gfx<%s> provided", TYPE_NAME<GFX>() );
          return false;
        }

        constexpr const bool is_valid_gfx_device  = std::is_convertible<GFX, LGFX_Device>::value;
        constexpr const bool is_valid_gfx_sprite  = std::is_convertible<GFX, LGFX_Sprite>::value;
        constexpr const bool is_valid_gfx         = is_valid_gfx_device || is_valid_gfx_sprite;

        if(!is_valid_gfx) {
          ESP_LOGE(PPA_TAG, "Unsupported GFX type: %s, accepted types are: LGFX_Device*, M5GFX*, LGFX_Sprite*, PPA_Sprite*", TYPE_NAME<GFX>() );
          static_assert(is_valid_gfx, "getBuffer(): Bad gfx type");
          return false;
        }

        if(! lgfx_ppa_get_bit_depth<GFX>((LGFX_Device*)gfx, bitDepth)) {
          return false;
        }

        if( is_valid_gfx_device ) {

          if(!getDeviceBuffer) {
            ESP_LOGE(PPA_TAG, "No device buffer callback was set for %s", TYPE_NAME<GFX>() );
            return false;
          }

          if(!getDeviceBuffer((LGFX_Device*)gfx, dst_buffer)) {
            ESP_LOGE(PPA_TAG, "Failed to get device buffer for %s", TYPE_NAME<GFX>() );
            return false;
          }

        } else if( is_valid_gfx_sprite ) { // input or output

          if(!getSpriteBuffer) {
            ESP_LOGE(PPA_TAG, "No sprite buffer callback was set for %s", TYPE_NAME<GFX>() );
            return false;
          }

          if( !getSpriteBuffer((LGFX_Device*)gfx, dst_buffer)) {
            return false;
          }

        }

        return true;
      }


    };




    // ---------------------------------------------------------------------------------------------



    class PPAFill : public PPABase
    {
    private:
      ppa_fill_oper_config_t oper_config =  ppa_fill_oper_config_t();

    public:

      ppa_fill_oper_config_t config() { return oper_config; }
      void config(ppa_fill_oper_config_t cfg) { oper_config=cfg; }
      ppa_fill_oper_config_t *configPtr() { return &oper_config; }

      template <typename GFX>
      PPAFill(GFX* out, bool async = false, bool use_semaphore = false, dev_buffer_provider_t getbuf = LGFX_PPA_DEVICE_BUFFER_PROVIDER)
      : PPABase(out, PPA_OPERATION_FILL, async, use_semaphore, getbuf)
      {
        if(!base_inited) {
          ESP_LOGE(FILL_TAG, "PPAFill can't init due to PPABase init fail");
          return;
        }
        enabled = true;
      }

      template <typename T>
      bool fillRect( uint32_t x, uint32_t y, uint32_t w, uint32_t h, const T& color )
      {
        if(!base_inited || !enabled)
          return false;

        clipRect_t outClipRect = { (int32_t)x, (int32_t)y, (int32_t)output_w, (int32_t)output_h };
        ppa_out_pic_blk_config_t out_cfg;
        if( !config_block_out(&out_cfg, output_buffer, output_buffer_size, outClipRect, output_bytes_per_pixel*8) )
          return false;

        oper_config = {
          .out             = out_cfg,
          .fill_block_w    = w,
          .fill_block_h    = h,
          .fill_argb_color = ppa_color_convert_argb8888(color),
          .mode            = async ? PPA_TRANS_MODE_NON_BLOCKING : PPA_TRANS_MODE_BLOCKING,
          .user_data       = (void*)this,
        };

        return PPABase::exec(&oper_config);
      }

    };



    // ---------------------------------------------------------------------------------------------


    class PPABlend : public PPABase
    {
    private:
      ppa_blend_oper_config_t oper_config = ppa_blend_oper_config_t();

    public:

      ppa_blend_oper_config_t config() { return oper_config; }
      void config(ppa_blend_oper_config_t cfg) { oper_config=cfg; }
      ppa_blend_oper_config_t *configPtr() { return &oper_config; }

      template <typename GFX>
      PPABlend(GFX* out, bool async = true, bool use_semaphore = false, dev_buffer_provider_t getbuf = LGFX_PPA_DEVICE_BUFFER_PROVIDER)
      : PPABase(out, PPA_OPERATION_BLEND, async, use_semaphore, getbuf)
      {
        if(!base_inited) {
          ESP_LOGE(BLEND_TAG, "PPABlend can't init due to PPABase init fail");
          return;
        }
        enabled = true;
        resetConfig();
      }

      bool pushImageBlend();

      void resetConfig();

      void invertBGFG(bool invert);

      void setFGRGBSwap(bool rgb_swap);
      void setFGByteSwap(bool byte_swap);
      void setFGAlpha(uint8_t alpha);
      void setFGColorKey(bool enable, uint32_t lo=0, uint32_t hi=0);

      void setBGRGBSwap(bool rgb_swap);
      void setBGByteSwap(bool byte_swap);
      void setBGAlpha(uint8_t alpha);
      void setBGColorKey(bool enable, uint32_t lo=0, uint32_t hi=0);


      template <typename GFX>
      bool setFG(GFX* fg)
      {
        if(!base_inited || !enabled)
          return false;
        if( !config_block_in(&oper_config.in_fg, fg) )
          return false;
        setFGByteSwap( fg->getSwapBytes() );
        return true;
      }


      template <typename GFX>
      bool setBG(GFX* bg)
      {
        if(!base_inited || !enabled)
          return false;
        if( !config_block_in(&oper_config.in_bg, bg) )
          return false;
        setBGByteSwap( bg->getSwapBytes() );
        return true;
      }


      template <typename FG, typename BG>
      bool setLayers(FG* fg, BG* bg)
      {
        if(!base_inited || !enabled)
          return false;
        if( fg->width() != output_w || fg->height() != output_h || bg->width() != output_w || bg->height() != output_h ) {
          ESP_LOGE(BLEND_TAG, "fg/bg Dimensions don't match!");
          return false; // FG and BG size must match the output block size
        }
        if(!setBG(bg))
          return false;
        if(!setFG(fg))
          return false;
        return true;
      }

      // blend with single color
      template <typename FG, typename BG, typename FGTransColor>
      bool pushImageBlend( FG* fg, BG* bg, FGTransColor fgtrans)
      {
        if(!base_inited || !enabled)
          return false;
        if( !setLayers(fg, bg) )
          return false;
        setFGColorKey(true, fgtrans,fgtrans);
        setFGAlpha(0xff);
        setBGColorKey(false);
        setBGAlpha(0xff);
        return pushImageBlend();
      }

      // blend with two colors
      template <typename FG, typename BG, typename FGTransColor, typename BGTransColor>
      bool pushImageBlend( FG* fg, FGTransColor fgtrans, BG* bg, BGTransColor bgtrans)
      {
        if(!base_inited || !enabled)
          return false;
        if( !setLayers(fg, bg) )
          return false;
        setFGColorKey(true, fgtrans,fgtrans);
        setFGAlpha(0xff);
        setBGColorKey(true, bgtrans,bgtrans);
        setBGAlpha(0xff);
        return pushImageBlend();
      }

      // blend with transparency
      template <typename FG, typename BG>
      bool pushImageBlendAlpha( FG* fg, float fg_alpha_float_val, BG* bg, float bg_alpha_float_val)
      {
        if(!base_inited || !enabled)
          return false;
        if( !setLayers(fg, bg) )
          return false;
        setFGColorKey(false);
        setFGAlpha(fg_alpha_float_val*0xff);
        setBGColorKey(false);
        setBGAlpha(bg_alpha_float_val*0xff);
        return pushImageBlend();
      }

    }; // end class PPABlend



    // ---------------------------------------------------------------------------------------------



    class PPASrm : public PPABase
    {

    private:
      ppa_srm_rotation_angle_t output_rotation = PPA_SRM_ROTATION_ANGLE_0;
      ppa_srm_oper_config_t oper_config = ppa_srm_oper_config_t();

    public:

      ppa_srm_oper_config_t config() { return oper_config; }
      void config(ppa_srm_oper_config_t cfg) { oper_config=cfg; }
      ppa_srm_oper_config_t *configPtr() { return &oper_config; }

      template <typename GFX>
      PPASrm(GFX* out, bool async = true, bool use_semaphore = false, dev_buffer_provider_t getbuf = LGFX_PPA_DEVICE_BUFFER_PROVIDER)
      : PPABase(out, PPA_OPERATION_SRM, async, use_semaphore, getbuf)
      {
        if(!base_inited) {
          ESP_LOGE(SRM_TAG, "PPASrm can't init due to PPABase init fail");
          return;
        }
        resetConfig();
        enabled = true;
      }

      void resetConfig();

      void setRGBSwap( bool rgb_swap );
      void setByteSwap( bool byte_swap );
      void setMirror( bool mirror_x, bool mirror_y );
      void setScale( float scale_x, float scale_y=0 );
      void setRotation( uint8_t rotation );
      void setAlpha(uint8_t alpha);

      bool pushImageSRM(uint32_t dst_x, uint32_t dst_y, uint32_t src_x, uint32_t src_y, float zoomx, float zoomy, uint32_t src_w, uint32_t src_h, void* buf, uint8_t bitDepth);

      template <typename T>
      bool pushImageSRM(uint32_t dst_x, uint32_t dst_y, uint32_t src_x, uint32_t src_y, uint8_t rot, float zoomx, float zoomy, uint32_t src_w, uint32_t src_h, const T* buf )
      {
        if(!base_inited || !enabled)
          return false;
        setRotation(rot);
        return pushImageSRM(dst_x, dst_y, src_x, src_y, zoomx, zoomy, src_w, src_h, (void*)buf, sizeof(T)*8);
      }

      template <typename GFX>
      bool pushSRM(GFX* input, float dst_x, float dst_y, float scale_x=1.0, float scale_y=1.0)
      {
        if(!base_inited || !enabled)
          return false;
        void* input_buffer;
        uint8_t bitDepth = 16;

        if( ! get_buffer(input, input_buffer, bitDepth) )
          return false;

        int32_t src_x, src_y, src_w, src_h;
        input->getClipRect(&src_x, &src_y, &src_w, &src_h); // NOTE: rotation already applied

        if( src_x+src_w-1 >= input->width() || src_y+src_h-1>=input->height() ) {
          ESP_LOGE(SRM_TAG, "input ClipRect {%d, %d, %d, %d} is outside its own boundaries", src_x, src_y, src_w, src_h);
          return false;
        }

        return pushImageSRM(dst_x, dst_y, src_x, src_y, scale_x, scale_y, src_w, src_h, input_buffer, bitDepth);
      }

    }; // end class PPASrm

  }

  using lgfx::PPA_Sprite;
  using lgfx::PPASrm;
  using lgfx::PPABlend;
  using lgfx::PPAFill;

#endif // defined defined LGFX_PPA_SUPPORTED
