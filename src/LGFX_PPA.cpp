/*\
 *
 * PPA operation utils for LovyanGFX/M5GFX
 *
 * An incomplete experiment brought to you by tobozo, copyleft (c+) Aug. 2025
 *
\*/
#include "LGFX_PPA.hpp"

#if defined SOC_MIPI_DSI_SUPPORTED && defined CONFIG_IDF_TARGET_ESP32P4


  namespace lgfx
  {


    // ---------------------------------------------------------------------------------------------

    void* heap_alloc_ppa(size_t length, size_t*size)
    {
      size_t cache_line_size;
      if(esp_cache_get_alignment(MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA, &cache_line_size)!=ESP_OK)
        return nullptr;
      assert(cache_line_size>=2);
      auto mask = cache_line_size-1;
      size_t aligned = (length + mask) & ~mask;
      auto ret = heap_caps_aligned_calloc(cache_line_size, 1, aligned, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
      if( size ) {
        if( ret )
          *size = aligned;
        else
          *size = 0;
      }
      return ret;
    }


    // ---------------------------------------------------------------------------------------------


    // on_trans_done callback when using a semaphore
    bool IRAM_ATTR lgfx_ppa_cb_sem_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data)
    {
      if( user_data ) {
        auto ppa_emitter = (PPABase*)user_data;
        ppa_emitter->setTransferDone(true);
        auto sem = ppa_emitter->getSemaphore();
        if( sem ) {
          BaseType_t xHigherPriorityTaskWoken = pdFALSE;
          xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
          return (xHigherPriorityTaskWoken == pdTRUE);
        }
      }
      ESP_LOGE(PPA_TAG, "ppa callback triggered without semaphore");
      return false;
    }

    // on_trans_done callback when *not* using a semaphore
    bool lgfx_ppa_cb_bool_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data)
    {
      if( user_data ) {
        auto ppa_emitter = (PPABase*)user_data;
        ppa_emitter->setTransferDone(true);
      }
      return false;
    }


    // ---------------------------------------------------------------------------------------------

    // a little debug helper
    const char* ppa_operation_type_to_string( ppa_operation_t oper_type )
    {
      static constexpr const char* srm     = "srm";
      static constexpr const char* blend   = "blend";
      static constexpr const char* fill    = "fill";
      static constexpr const char* unknown = "unknown";
      switch(oper_type)
      {
        case PPA_OPERATION_BLEND: return blend; break;
        case PPA_OPERATION_SRM:   return srm;   break;
        case PPA_OPERATION_FILL:  return fill;  break;
        default:break;
      }
      return unknown;
    }


    // ---------------------------------------------------------------------------------------------

    ppa_fill_color_mode_t ppa_fill_color_mode(uint8_t lgfx_color_bit_depth)
    {
      if( lgfx_color_bit_depth==16 )
        return PPA_FILL_COLOR_MODE_RGB565;
      if( lgfx_color_bit_depth==24 )
        return PPA_FILL_COLOR_MODE_RGB888;
      if( lgfx_color_bit_depth==32 )
        return PPA_FILL_COLOR_MODE_ARGB8888;
      return PPA_FILL_COLOR_MODE_RGB565;
    }


    ppa_blend_color_mode_t ppa_blend_color_mode(uint8_t lgfx_color_bit_depth)
    {
      if( lgfx_color_bit_depth==16 )
        return PPA_BLEND_COLOR_MODE_RGB565;
      if( lgfx_color_bit_depth==24 )
        return PPA_BLEND_COLOR_MODE_RGB888;
      if( lgfx_color_bit_depth==32 )
        return PPA_BLEND_COLOR_MODE_ARGB8888;
      return PPA_BLEND_COLOR_MODE_RGB565;
    }


    ppa_srm_color_mode_t ppa_srm_color_mode(uint8_t lgfx_color_bit_depth)
    {
      if( lgfx_color_bit_depth==16 )
        return PPA_SRM_COLOR_MODE_RGB565;
      if( lgfx_color_bit_depth==24 )
        return PPA_SRM_COLOR_MODE_RGB888;
      if( lgfx_color_bit_depth==32 )
        return PPA_SRM_COLOR_MODE_ARGB8888;
      return PPA_SRM_COLOR_MODE_RGB565;
    }


    // ---------------------------------------------------------------------------------------------

    ppa_srm_rotation_angle_t ppa_srm_get_rotation_from_angle(float angle)
    {
      int rotation = fmod(angle, 360.0) / 90; // only 4 angles supported
      switch(rotation)
      {
        case 0: return PPA_SRM_ROTATION_ANGLE_0;
        case 1: return PPA_SRM_ROTATION_ANGLE_90;
        case 2: return PPA_SRM_ROTATION_ANGLE_180;
        case 3: return PPA_SRM_ROTATION_ANGLE_270;
      }
      return PPA_SRM_ROTATION_ANGLE_0;
    }


    float ppa_srm_get_angle_from_rotation(ppa_srm_rotation_angle_t rotation)
    {
      switch(rotation)
      {
        case PPA_SRM_ROTATION_ANGLE_0  : return 0  ;
        case PPA_SRM_ROTATION_ANGLE_90 : return 90 ;
        case PPA_SRM_ROTATION_ANGLE_180: return 180;
        case PPA_SRM_ROTATION_ANGLE_270: return 270;
      }
      return 0;
    }


    // ---------------------------------------------------------------------------------------------



    void* PPA_Sprite::createSprite(int32_t w, int32_t h)
    {
      uint8_t bit_depth  = getColorDepth();
      uint8_t byte_depth = bit_depth/8;

      if(byte_depth<2 || byte_depth>4) {
        ESP_LOGE(PPA_SPRITE_TAG, "Only 16/24/32 bits colors are supported");
        return nullptr;
      }

      if(w<=0 || h<=0) {
        ESP_LOGE(PPA_SPRITE_TAG, "Invalid requested dimensions: [%d*%d]", w, h);
        return nullptr;
      }

      void* buf = lgfx::heap_alloc_ppa(w * h * byte_depth);

      if(!buf)
        return nullptr;

      setBuffer(buf, w, h, bit_depth);
      return _img;
    }


    // ---------------------------------------------------------------------------------------------



    PPABase::~PPABase()
    {
      if (async && use_semaphore && ppa_semaphore) {
        if( xSemaphoreTake(ppa_semaphore, ( TickType_t )10 ) != pdTRUE ) {
          ESP_LOGE(PPA_TAG, "Failed to take semaphore before deletion, incoming crash..?");
        }
      }

      if( ppa_client_handle )
        ppa_unregister_client(ppa_client_handle);
    }


    void PPABase::setTransferDone(bool val)
    {
      ppa_transfer_done = val;
    }


    SemaphoreHandle_t PPABase::getSemaphore()
    {
      return ppa_semaphore;
    }


    bool PPABase::available()
    {
      if(!enabled) {
        ESP_LOGE(PPA_TAG, "Call to available() when ppa_%s is disabled!", ppa_operation_type_to_string(ppa_client_config.oper_type) );
        return false;
      }
      return ppa_transfer_done;
    }


    bool PPABase::ready()
    {
      if(!available()) {
        ESP_LOGV(PPA_TAG, "Skipping exec");
        return false;
      }

      if (async && use_semaphore) {
        if( xSemaphoreTake(ppa_semaphore, ( TickType_t )10 ) != pdTRUE ) {
          ESP_LOGE(PPA_TAG, "Failed to take semaphore");
          return false;
        }
      }

      setTransferDone(false);

      return true;
    }



    bool PPABase::config_block_in(ppa_in_pic_blk_config_t* cfg, void*buffer, uint32_t w, uint32_t h, clipRect_t clipRect, uint8_t bitDepth)
    {
      if( !cfg || !buffer ) // malformed call
        return false;
      if( clipRect.x+clipRect.w>w || clipRect.y+clipRect.h>h || clipRect.w<=0 || clipRect.h<=0 || clipRect.x<0 || clipRect.y<0) {
        ESP_LOGE(PPA_TAG, "ClipRect {%d, %d, %d, %d} is outside boundaries", clipRect.x, clipRect.y, clipRect.w, clipRect.h);
        return false; // clipRect must fit in buffer area
      }

      *cfg = {
        .buffer         = buffer,
        .pic_w          = w,
        .pic_h          = h,
        .block_w        = (uint32_t)clipRect.w,
        .block_h        = (uint32_t)clipRect.h,
        .block_offset_x = (uint32_t)clipRect.x,
        .block_offset_y = (uint32_t)clipRect.y,
        .blend_cm       = ppa_blend_color_mode(bitDepth),
        .yuv_range      = ppa_color_range_t(),
        .yuv_std        = ppa_color_conv_std_rgb_yuv_t()
      };
      return true;
    }


    bool PPABase::config_block_out(ppa_out_pic_blk_config_t *cfg, void*buffer, uint32_t buffer_size, clipRect_t clipRect, uint8_t bitDepth)
    {
      if( !cfg || !buffer ) // malformed call
        return false;
      if( clipRect.w<=0 || clipRect.h<=0 || clipRect.x<0 || clipRect.y<0 || clipRect.x>clipRect.w-1 || clipRect.y>clipRect.h-1 )
        return false;

      *cfg =
      {
        .buffer         = buffer,
        .buffer_size    = buffer_size,
        .pic_w          = (uint32_t)clipRect.w,
        .pic_h          = (uint32_t)clipRect.h,
        .block_offset_x = (uint32_t)clipRect.x,
        .block_offset_y = (uint32_t)clipRect.y,
        .blend_cm       = ppa_blend_color_mode(bitDepth),
        .yuv_range      = ppa_color_range_t(),
        .yuv_std        = ppa_color_conv_std_rgb_yuv_t()
      };
      return true;
    }


    // ---------------------------------------------------------------------------------------------


    void PPASrm::resetConfig()
    {
      // NOTE: ppa rotation values are counter-clockwise while LGFX rotation values are clockwise
      output_rotation = ppa_srm_get_rotation_from_angle( 360-(outputGFX->getRotation()%4)*90 );
      oper_config.rgb_swap  = false;
      oper_config.mirror_x  = false;
      oper_config.mirror_y  = false;
      oper_config.byte_swap = is_panel;
      oper_config.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
      oper_config.alpha_fix_val     = 0xff; // or .alpha_scale_ratio = 1.0f,
      oper_config.mode              = async ? PPA_TRANS_MODE_NON_BLOCKING : PPA_TRANS_MODE_BLOCKING;
      oper_config.user_data         = (void*)this;
    }


    void PPASrm::setAlpha(uint8_t alpha)
    {
      oper_config.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
      oper_config.alpha_fix_val     = 0xff;
    }



    void PPASrm::setScale( float scale_x, float scale_y )
    {
      if( scale_x==0 )
        scale_x = 1;
      if( scale_y==0 )
        scale_y=scale_x;

      oper_config.scale_x = scale_x;
      oper_config.scale_y = scale_y;
    }


    void PPASrm::setRGBSwap( bool rgb_swap )
    {
      oper_config.rgb_swap = rgb_swap;
    }


    void PPASrm::setByteSwap( bool byte_swap )
    {
      oper_config.byte_swap = byte_swap;
    }


    void PPASrm::setMirror( bool mirror_x, bool mirror_y )
    {
      oper_config.mirror_x = mirror_x;
      oper_config.mirror_y = mirror_y;
    }


    void PPASrm::setRotation( uint8_t rotation )
    {
      auto input_rotation  = ppa_srm_get_rotation_from_angle(rotation*90);
      auto output_angle    = ppa_srm_get_angle_from_rotation(output_rotation) + ppa_srm_get_angle_from_rotation(input_rotation);
      oper_config.rotation_angle = ppa_srm_get_rotation_from_angle(output_angle);
    }


    bool PPASrm::pushImageSRM(
      uint32_t dst_x, uint32_t dst_y,
      uint32_t src_x, uint32_t src_y,
      uint8_t rotation,
      float scale_x, float scale_y,
      uint32_t src_w, uint32_t src_h,
      void* input_buffer, uint8_t bitDepth
    ) {
      if(!inited)
        return false;

      if( bitDepth<16 || bitDepth>32 ) {
        ESP_LOGE(SRM_TAG, "Only 16/24/32bits colors supported");
        return false;
      }
      if(!input_buffer) {
        ESP_LOGE(SRM_TAG, "Buffer missing");
        enabled = false;
        return false;
      }

      if(  src_w==0 || src_h==0 || src_w<=src_x || src_h<=src_y ) {
        ESP_LOGE(SRM_TAG, "Bad dimensions");
        enabled = false;
        return false;
      }

      clipRect_t clipRectIn = {(int32_t)src_x, (int32_t)src_y, (int32_t)(src_w-src_x), (int32_t)(src_h-src_y)};
      if( !config_block_in(&oper_config.in, (void*)input_buffer, src_w, src_h, clipRectIn, bitDepth) )
        return false;

      clipRect_t clipRectOut = {(int32_t)dst_x, (int32_t)dst_y, (int32_t)output_w, (int32_t)output_h };
      if( !config_block_out(&oper_config.out, output_buffer, output_buffer_size, clipRectOut, output_bytes_per_pixel*8) )
        return false;

      setScale(scale_x, scale_y);
      setRotation(rotation);
      setAlpha(0xff);

      if( output_rotation%2==1 ) {
        std::swap( oper_config.out.pic_w, oper_config.out.pic_h );
        std::swap( oper_config.out.block_offset_x, oper_config.out.block_offset_y );
      }

      enabled = true;

      return PPABase::exec(&oper_config);
    }



    // ---------------------------------------------------------------------------------------------


    bool PPABlend::pushImageBlend()
    {
      if(!inited) {
        ESP_LOGE(BLEND_TAG, "Can't push after a failed init");
        return false;
      }

      clipRect_t outClipRect = { (int32_t)oper_config.in_bg.block_offset_x, (int32_t)oper_config.in_bg.block_offset_y, (int32_t)output_w, (int32_t)output_h };

      if( !config_block_out(&oper_config.out, output_buffer, output_buffer_size, outClipRect, output_bytes_per_pixel*8) )
        return false;

      return PPABase::exec(&oper_config);
    }


    void PPABlend::resetConfig()
    {
      oper_config.fg_rgb_swap          = false;
      oper_config.fg_byte_swap         = false;
      oper_config.fg_ck_en             = false;
      oper_config.bg_rgb_swap          = false;
      oper_config.bg_byte_swap         = false;
      oper_config.bg_ck_en             = false;
      oper_config.ck_reverse_bg2fg     = false;
      oper_config.mode                 = async ? PPA_TRANS_MODE_NON_BLOCKING : PPA_TRANS_MODE_BLOCKING;
      oper_config.user_data            = (void*)this;
      oper_config.fg_fix_rgb_val       = ppa_color_convert_rgb888(0);
      oper_config.ck_rgb_default_val   = ppa_color_convert_rgb888(0);
      oper_config.fg_alpha_update_mode = PPA_ALPHA_FIX_VALUE;
      oper_config.bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE;
    }


    void PPABlend::invertBGFG(bool invert)
    {
      oper_config.ck_reverse_bg2fg = invert;
    }


    void PPABlend::setFGRGBSwap(bool rgb_swap)
    {
      oper_config.fg_rgb_swap = rgb_swap;
    }


    void PPABlend::setFGByteSwap(bool byte_swap)
    {
      oper_config.fg_byte_swap = byte_swap;
    }


    void PPABlend::setFGAlpha(uint8_t alpha)
    {
      oper_config.fg_alpha_fix_val = alpha;
      oper_config.fg_alpha_update_mode = PPA_ALPHA_FIX_VALUE;
    }


    void PPABlend::setFGColorKey(bool enable, uint32_t lo, uint32_t hi)
    {
      oper_config.fg_ck_en = enable;
      oper_config.fg_ck_rgb_low_thres  = ppa_color_convert_rgb888(lo);
      oper_config.fg_ck_rgb_high_thres = ppa_color_convert_rgb888(hi);
    }


    void PPABlend::setBGRGBSwap(bool rgb_swap )
    {
      oper_config.bg_rgb_swap = rgb_swap;
    }


    void PPABlend::setBGByteSwap(bool byte_swap )
    {
      oper_config.bg_byte_swap = byte_swap;
    }


    void PPABlend::setBGAlpha(uint8_t alpha)
    {
      oper_config.bg_alpha_fix_val = alpha;
      oper_config.bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE;
    }


    void PPABlend::setBGColorKey(bool enable, uint32_t lo, uint32_t hi)
    {
      oper_config.bg_ck_en = enable;
      oper_config.bg_ck_rgb_low_thres  = ppa_color_convert_rgb888(lo);
      oper_config.bg_ck_rgb_high_thres = ppa_color_convert_rgb888(hi);
    }



    // ---------------------------------------------------------------------------------------------


  }; // end namespace lgfx

#endif
