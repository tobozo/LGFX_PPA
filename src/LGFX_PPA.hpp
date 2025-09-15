/*\
 *
 * PPA operation utils for LovyanGFX/M5GFX
 *
 * An incomplete experiment brought to you by tobozo, copyleft (c+) Aug. 2025
 *
\*/
#pragma once

#if __has_include(<M5GFX.h>) || __has_include(<M5Unified.hpp>)
  #include <M5GFX.h>
  #include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>
  using m5gfx::Panel_DSI;
  #define GFX_BASE M5GFX
#elif __has_include(<LovyanGFX.hpp>)
  #include <LovyanGFX.hpp>
  #include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>
  using lgfx::Panel_DSI;
  #define GFX_BASE LovyanGFX
#else
  #error "Please include M5GFX.h, M5Unified.hpp or LovyanGFX.hpp before including this file"
#endif

#if !defined SOC_MIPI_DSI_SUPPORTED || !defined CONFIG_IDF_TARGET_ESP32P4
  #error "PPA is only available with ESP32P4, and this implementation depends on MIPI/DSI support"
#else

  extern "C"
  {
    #include "driver/ppa.h"
    #include "esp_heap_caps.h"
    #include "esp_cache.h"
    #include "esp_private/esp_cache_private.h"
    #include "esp_log.h"

    // some macros grabbed from LVGL

    #define LGFX_PPA_ALIGN_UP(x, align)  ((((x) + (align) - 1) / (align)) * (align))
    #define LGFX_PPA_PTR_ALIGN_UP(p, align) ((void*)(((uintptr_t)(p) + (uintptr_t)((align) - 1)) & ~(uintptr_t)((align) - 1)))

    #define LGFX_PPA_ALIGN_DOWN(x, align)  ((((x) - (align) - 1) / (align)) * (align))
    #define LGFX_PPA_PTR_ALIGN_DOWN(p, align) ((void*)(((uintptr_t)(p) - (uintptr_t)((align) - 1)) & ~(uintptr_t)((align) - 1)))

    #define LGFX_PPA_RESET_CACHE(d, s) esp_cache_msync( \
          (void *)LGFX_PPA_PTR_ALIGN_DOWN(d, CONFIG_CACHE_L1_CACHE_LINE_SIZE), \
          LGFX_PPA_ALIGN_DOWN(s, CONFIG_CACHE_L1_CACHE_LINE_SIZE), \
          ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA \
        );
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

    // on_trans_done() callbacks
    bool lgfx_ppa_cb_sem_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data);
    bool lgfx_ppa_cb_bool_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data);

    // a little debug helper
    const char* ppa_operation_type_to_string( ppa_operation_t oper_type );
    ppa_fill_color_mode_t ppa_fill_color_mode(uint8_t lgfx_color_bit_depth);

    ppa_blend_color_mode_t ppa_blend_color_mode(uint8_t lgfx_color_bit_depth);
    ppa_srm_color_mode_t ppa_srm_color_mode(uint8_t lgfx_color_bit_depth);

    ppa_srm_rotation_angle_t ppa_srm_get_rotation_from_angle(float angle);
    float ppa_srm_get_angle_from_rotation(ppa_srm_rotation_angle_t rotation);


    template<typename T>
    color_pixel_rgb888_data_t ppa_color_convert_rgb888(T c)
    {
      rgb888_t ct = c; // NOTE: implicit conversion
      return {.b=ct.B8(), .g=ct.G8(), .r=ct.R8() };
    }

    template<typename T>
    color_pixel_argb8888_data_t ppa_color_convert_argb8888(T c)
    {
      argb8888_t ct = c; // NOTE: implicit conversion
      return {.b=ct.B8(), .g=ct.G8(), .r=ct.R8(), .a=ct.A8() };
    }


    struct clipRect_t { int32_t x; int32_t y; int32_t w; int32_t h; };


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
      PPABase(GFX* out, ppa_operation_t oper_type, bool async = true, bool use_semaphore = false)
      : ppa_client_config({.oper_type=oper_type, .max_pending_trans_num=1, .data_burst_length=PPA_DATA_BURST_LENGTH_128}),
        async(async), use_semaphore(use_semaphore), output_w(out->width()), output_h(out->height()), outputGFX((GFX_BASE*)out),
        is_panel( std::is_same<GFX, GFX_BASE>::value )
      {
        enabled = false;

        if( output_w==0 || output_h==0 ) {
          ESP_LOGE(PPA_TAG, "Bad output w/h");
          return;
        }

        if(!async && use_semaphore) {
          ESP_LOGW(PPA_TAG, "use_semaphore=true but async=false, async will be enabled anyway");
          async = true;
        }

        if( ppa_semaphore == NULL )
          ppa_semaphore = xSemaphoreCreateBinary();

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
          return;
        }

        inited = true;
      }


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

      ppa_client_config_t ppa_client_config;
      ppa_client_handle_t ppa_client_handle = nullptr;
      ppa_event_callbacks_t ppa_event_cb;

      volatile bool ppa_transfer_done = true;

      bool inited  = false;
      bool enabled = true;

      const bool async;
      const bool use_semaphore;

      const uint32_t output_w;
      const uint32_t output_h;

      uint8_t output_bytes_per_pixel;

      void* output_buffer = nullptr;
      uint32_t output_buffer_size;// = LGFX_PPA_ALIGN_UP(output_w*output_h*output_bytes_per_pixel, CONFIG_CACHE_L1_CACHE_LINE_SIZE);

      GFX_BASE* outputGFX;

      const bool is_panel;

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

        if( std::is_same<GFX, GFX_BASE>::value ) {
          buf = ((Panel_DSI*)outputGFX->getPanel())->config_detail().buffer; // assuming 16bpp
        } else if( std::is_same<GFX, LGFX_Sprite>::value || std::is_same<GFX, PPA_Sprite>::value  ) {
          buf = ((LGFX_Sprite*)gfx)->getBuffer();
          bitDepth = ((LGFX_Sprite*)gfx)->getColorDepth();
          if( bitDepth < 16 ) {
            ESP_LOGE(PPA_TAG, "Unsupported input bit depth: %d", bitDepth );
            return false;
          }
        } else {
          ESP_LOGE(PPA_TAG, "Unsupported gfx type: %s", TYPE_NAME<GFX>() );
          return false;
        }

        clipRect_t clipRect = {0,0,0,0};
        gfx->getClipRect(&clipRect.x, &clipRect.y, &clipRect.w, &clipRect.h);

        return config_block_in(cfg, buf, gfx->width(), gfx->height(), clipRect, bitDepth);
      }


      template <typename GFX>
      bool config_block_out(ppa_out_pic_blk_config_t *cfg)
      {
        if(!cfg)
          return false;
        uint8_t bitDepth = 16;
        if( std::is_same<GFX, GFX_BASE>::value ) {
          output_buffer = ((Panel_DSI*)outputGFX->getPanel())->config_detail().buffer;
          output_bytes_per_pixel = 2; // panelDSI->getColorDepth() returns a weird value, so 16bits colors it is...
        } else if( std::is_same<GFX, LGFX_Sprite>::value || std::is_same<GFX, PPA_Sprite>::value  ) {
          bitDepth = outputGFX->getColorDepth();
          if( bitDepth < 16 ) {
            ESP_LOGE(PPA_TAG, "Unsupported bit depth: %d", bitDepth );
            return false;
          }
          output_bytes_per_pixel = bitDepth/8;
          output_buffer = ((LGFX_Sprite*)outputGFX)->getBuffer();
        } else {
          ESP_LOGE(PPA_TAG, "Unsupported GFX type: %s, accepted types are: LovyanGFX*, M5GFX*, LGFX_Sprite*, PPA_Sprite*", TYPE_NAME<GFX>() );
          return false;
        }

        clipRect_t clipRect = {0,0,0,0};
        outputGFX->getClipRect(&clipRect.x, &clipRect.y, &clipRect.w, &clipRect.h);

        output_buffer_size = LGFX_PPA_ALIGN_UP(clipRect.w*clipRect.h*output_bytes_per_pixel, CONFIG_CACHE_L1_CACHE_LINE_SIZE);

        return config_block_out(cfg, output_buffer, clipRect.w*clipRect.h*output_bytes_per_pixel, clipRect, bitDepth );
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

      template <typename GFX>
      PPAFill(GFX* out, bool async = false, bool use_semaphore = false)
      : PPABase(out, PPA_OPERATION_FILL, async, use_semaphore)
      { enabled = true; }

      template <typename T>
      bool fillRect( uint32_t x, uint32_t y, uint32_t w, uint32_t h, const T& color )
      {
        if(!inited)
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

      template <typename GFX>
      PPABlend(GFX* out, bool async = true, bool use_semaphore = false)
      : PPABase(out, PPA_OPERATION_BLEND, async, use_semaphore)
      {
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
        if( !config_block_in(&oper_config.in_fg, fg) )
          return false;
        setFGByteSwap( fg->getSwapBytes() );
        return true;
      }


      template <typename GFX>
      bool setBG(GFX* bg)
      {
        if( !config_block_in(&oper_config.in_bg, bg) )
          return false;
        setBGByteSwap( bg->getSwapBytes() );
        return true;
      }


      template <typename FG, typename BG>
      bool setLayers(FG* fg, BG* bg)
      {
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

      template <typename GFX>
      PPASrm(GFX* out, bool async = true, bool use_semaphore = false)
      : PPABase(out, PPA_OPERATION_SRM, async, use_semaphore)
      {
        if(!inited)
          return;
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

      bool pushImageSRM(uint32_t dst_x, uint32_t dst_y, uint32_t src_x, uint32_t src_y, uint8_t rot, float zoomx, float zoomy, uint32_t src_w, uint32_t src_h, void* buf, uint8_t bitDepth);

      template <typename T>
      bool pushImageSRM(uint32_t dst_x, uint32_t dst_y, uint32_t src_x, uint32_t src_y, uint8_t rot, float zoomx, float zoomy, uint32_t src_w, uint32_t src_h, const T* buf )
      {
        return pushImageSRM(dst_x, dst_y, src_x, src_y, rot, zoomx, zoomy, src_w, src_h, (void*)buf, sizeof(T)*8);
      }

      template <typename GFX>
      bool pushSRM(GFX* input, float dst_x, float dst_y, float scale_x=1.0, float scale_y=1.0)
      {
        void* input_buffer;
        uint8_t bitDepth = 16;

        if( std::is_same<GFX, GFX_BASE>::value ) {
          auto base = (GFX_BASE*)input;
          auto panelDSI = (Panel_DSI*)base->getPanel();
          input_buffer = panelDSI->config_detail().buffer;
        } else if( std::is_same<GFX, LGFX_Sprite>::value || std::is_same<GFX, PPA_Sprite>::value  ) {
          auto sprite = (LGFX_Sprite*)input;
          if( sprite->getColorDepth() < 16 ) {
            ESP_LOGE(SRM_TAG, "Unsupported bit depth: %d", sprite->getColorDepth() );
            return false;
          }
          bitDepth = sprite->getColorDepth();
          input_buffer = sprite->getBuffer();
        } else {
          ESP_LOGE(SRM_TAG, "Unsupported GFX type: %s, accepted types are: LovyanGFX*, M5GFX*, LGFX_Sprite*, PPA_Sprite*", TYPE_NAME<GFX>() );
          return false;
        }

        int32_t src_x, src_y, src_w, src_h;
        input->getClipRect(&src_x, &src_y, &src_w, &src_h); // NOTE: rotation already applied

        if( src_x+src_w-1 >= input->width() || src_y+src_h-1>=input->height() ) {
          ESP_LOGE(SRM_TAG, "input ClipRect {%d, %d, %d, %d} is outside its own boundaries", src_x, src_y, src_w, src_h);
          return false;
        }

        auto input_rotation = input->getRotation();

        // don't translate rotation twice
        if( input_rotation%2 == 1 )
          std::swap(src_w, src_h);

        return pushImageSRM(dst_x, dst_y, src_x, src_y, input_rotation, scale_x, scale_y, src_w, src_h, input_buffer, bitDepth);
      }

    }; // end class PPASrm

  }

  using lgfx::PPA_Sprite;
  using lgfx::PPASrm;
  using lgfx::PPABlend;
  using lgfx::PPAFill;

#endif
