#include "pebble.h"

static Window *window;
static Layer *window_layer;

GColor background_color = GColorBlack;

//static AppSync sync;
//static uint8_t sync_buffer[100];

static bool appStarted = false;

static int invert;
static int bluetoothvibe;
static int hourlyvibe;

/*enum {
  INVERT_COLOR_KEY = 0x0,
  HOURLYVIBE_KEY = 0x1,
  BLUETOOTHVIBE_KEY = 0x2
};
*/
TextLayer *battery_text_layer;
TextLayer *layer_date_text;

static GBitmap *bluetooth_image;
static BitmapLayer *bluetooth_layer;

static GBitmap *background_image;
static BitmapLayer *background_layer;

static GFont *date_font;

#define TOTAL_TIME_DIGITS 4
static GBitmap *time_digits_images[TOTAL_TIME_DIGITS];
static BitmapLayer *time_digits_layers[TOTAL_TIME_DIGITS];

const int BIG_DIGIT_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_NUM_0,
  RESOURCE_ID_IMAGE_NUM_1,
  RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3,
  RESOURCE_ID_IMAGE_NUM_4,
  RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6,
  RESOURCE_ID_IMAGE_NUM_7,
  RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9
};

int charge_percent = 0;
InverterLayer *inverter_layer = NULL;

/*
void set_invert_color(bool invert) {
  if (invert && inverter_layer == NULL) {
    // Add inverter layer
    Layer *window_layer = window_get_root_layer(window);
    inverter_layer = inverter_layer_create(GRect(0, 0, 144, 168));
    layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));
  } else if (!invert && inverter_layer != NULL) {
    // Remove Inverter layer
    layer_remove_from_parent(inverter_layer_get_layer(inverter_layer));
    inverter_layer_destroy(inverter_layer);
    inverter_layer = NULL;
  }
  // No action required
}
*/
static void handle_tick(struct tm *tick_time, TimeUnits units_changed);
/*
static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
   
	switch (key) {

	case INVERT_COLOR_KEY:
      invert = new_tuple->value->uint8 != 0;
	  persist_write_bool(INVERT_COLOR_KEY, invert);
      set_invert_color(invert);
      break;
    case BLUETOOTHVIBE_KEY:
      bluetoothvibe = new_tuple->value->uint8 != 0;
	  persist_write_bool(BLUETOOTHVIBE_KEY, bluetoothvibe);
      break;      
    case HOURLYVIBE_KEY:
      hourlyvibe = new_tuple->value->uint8 != 0;
	  persist_write_bool(HOURLYVIBE_KEY, hourlyvibe);	  
      break; 
  }
}
*/
static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin) {
  GBitmap *old_image = *bmp_image;
  *bmp_image = gbitmap_create_with_resource(resource_id);
  GRect frame = (GRect) {
    .origin = origin,
    .size = (*bmp_image)->bounds.size
  };
  bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
  layer_set_frame(bitmap_layer_get_layer(bmp_layer), frame);
  if (old_image != NULL) {
	gbitmap_destroy(old_image);
	old_image = NULL;
  }
}

void update_battery_state(BatteryChargeState charge_state) {
    static char battery_text[] = "x100%";

    if (charge_state.is_charging) {
        snprintf(battery_text, sizeof(battery_text), "+%d%%", charge_state.charge_percent);
    } else {
        snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
    }
    charge_percent = charge_state.charge_percent;
    text_layer_set_text(battery_text_layer, battery_text);
}

static void toggle_bluetooth_icon(bool connected) {

    if (connected) {
	  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), false);
    } else {
  	  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), true);
    }

    if (!connected && appStarted && bluetoothvibe) {
        vibes_long_pulse();
	}
}

void bluetooth_connection_callback(bool connected) {
  toggle_bluetooth_icon(connected);
}

void force_update(void) {
    update_battery_state(battery_state_service_peek());
    toggle_bluetooth_icon(bluetooth_connection_service_peek());
}

unsigned short get_display_hour(unsigned short hour) {
  if (clock_is_24h_style()) {
    return hour;
  }
  unsigned short display_hour = hour % 12;
  // Converts "0" to "12"
  return display_hour ? display_hour : 12;
}

static void update_hours(struct tm *tick_time) {

  if(appStarted && hourlyvibe) {
    //vibe!
    vibes_short_pulse();
  }
  
  unsigned short display_hour = get_display_hour(tick_time->tm_hour);

  set_container_image(&time_digits_images[0], time_digits_layers[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour/10], GPoint(31, 5));
  set_container_image(&time_digits_images[1], time_digits_layers[1], BIG_DIGIT_IMAGE_RESOURCE_IDS[display_hour%10], GPoint(87, 5));

  if (!clock_is_24h_style()) {
    
    if (display_hour/10 == 0) {
      layer_set_hidden(bitmap_layer_get_layer(time_digits_layers[0]), true);
    }
    else {
      layer_set_hidden(bitmap_layer_get_layer(time_digits_layers[0]), false);
    }
  }
}

static void update_minutes(struct tm *tick_time) {
  set_container_image(&time_digits_images[2], time_digits_layers[2], BIG_DIGIT_IMAGE_RESOURCE_IDS[tick_time->tm_min/10], GPoint(31, 93));
  set_container_image(&time_digits_images[3], time_digits_layers[3], BIG_DIGIT_IMAGE_RESOURCE_IDS[tick_time->tm_min%10], GPoint(87, 93));
}

static void update_month (struct tm *tick_time) {

    static char date_text[] = "00/xx";
	
    strftime(date_text, sizeof(date_text), "%e/%m", tick_time);	
   	text_layer_set_text(layer_date_text, date_text);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (units_changed & DAY_UNIT) {
    update_month(tick_time);
  }
  if (units_changed & HOUR_UNIT) {
    update_hours(tick_time);
  }
  if (units_changed & MINUTE_UNIT) {
   update_minutes(tick_time);
  }		
}

static void init(void) {
  memset(&time_digits_layers, 0, sizeof(time_digits_layers));
  memset(&time_digits_images, 0, sizeof(time_digits_images));

//  const int inbound_size = 64;
//  const int outbound_size = 64;
//  app_message_open(inbound_size, outbound_size);  

  window = window_create();
  if (window == NULL) {
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "OOM: couldn't allocate window");
      return;
  }
  window_stack_push(window, true /* Animated */);
  window_layer = window_get_root_layer(window);
  
  background_color  = GColorWhite;
  window_set_background_color(window, background_color);
	
  background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG);
  background_layer = bitmap_layer_create(layer_get_frame(window_layer));
  bitmap_layer_set_bitmap(background_layer, background_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(background_layer));	
  
  // Create time and date layers
  GRect dummy_frame = { {0, 0}, {0, 0} };
  
  for (int i = 0; i < TOTAL_TIME_DIGITS; ++i) {
    time_digits_layers[i] = bitmap_layer_create(dummy_frame);
  GCompOp compositing_mode = GCompOpAssign;
  bitmap_layer_set_compositing_mode(time_digits_layers[i], compositing_mode);	
  layer_add_child(window_layer, bitmap_layer_get_layer(time_digits_layers[i]));
  }

  date_font  = fonts_load_custom_font( resource_get_handle( RESOURCE_ID_FONT_CUSTOM_20 ) );

  battery_text_layer = text_layer_create(GRect(84, 70, 58, 22));
  text_layer_set_background_color(battery_text_layer, GColorClear);
  text_layer_set_text_color(battery_text_layer, GColorBlack);
  text_layer_set_text_alignment(battery_text_layer, GTextAlignmentRight);
  text_layer_set_font(battery_text_layer, date_font);
  layer_add_child(window_layer, text_layer_get_layer(battery_text_layer));
	
  layer_date_text = text_layer_create(GRect(2, 70, 140, 22));
  text_layer_set_background_color(layer_date_text, GColorClear);
  text_layer_set_text_color(layer_date_text, GColorBlack);
  text_layer_set_text_alignment(layer_date_text, GTextAlignmentLeft);
  text_layer_set_font(layer_date_text, date_font);
  layer_add_child(window_layer, text_layer_get_layer(layer_date_text));
	
  bluetooth_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH);
  GRect frame = (GRect) {
    .origin = { .x = 73, .y = 77},
    .size = bluetooth_image->bounds.size
  };
  bluetooth_layer = bitmap_layer_create(frame);
  bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(bluetooth_layer));
 
/*	
  Tuplet initial_values[] = {
    TupletInteger(INVERT_COLOR_KEY, persist_read_bool(INVERT_COLOR_KEY)),
    TupletInteger(HOURLYVIBE_KEY, persist_read_bool(HOURLYVIBE_KEY)),
    TupletInteger(BLUETOOTHVIBE_KEY, persist_read_bool(BLUETOOTHVIBE_KEY)),
  };
  
  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, NULL, NULL);
   */
  appStarted = true;
  
  // Avoids a blank screen on watch start.
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);  
  handle_tick(tick_time, DAY_UNIT + HOUR_UNIT + MINUTE_UNIT);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  bluetooth_connection_service_subscribe(bluetooth_connection_callback);
  battery_state_service_subscribe(&update_battery_state);

  // draw first frame
  force_update();

}

static void deinit(void) {

//  app_sync_deinit(&sync);
  
  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
	
  layer_remove_from_parent(bitmap_layer_get_layer(bluetooth_layer));
  bitmap_layer_destroy(bluetooth_layer);
  gbitmap_destroy(bluetooth_image);
  bluetooth_image = NULL;
	
  layer_remove_from_parent(bitmap_layer_get_layer(background_layer));
  bitmap_layer_destroy(background_layer);
  gbitmap_destroy(background_image);
  background_image = NULL;
	
	
  for (int i = 0; i < TOTAL_TIME_DIGITS; i++) {
  layer_remove_from_parent(bitmap_layer_get_layer(time_digits_layers[i]));
  gbitmap_destroy(time_digits_images[i]);
  time_digits_images[i] = NULL;
  bitmap_layer_destroy(time_digits_layers[i]);
  time_digits_layers[i] = NULL; 
  }

  text_layer_destroy( layer_date_text );
  text_layer_destroy( battery_text_layer );

  fonts_unload_custom_font( date_font );

  layer_remove_from_parent(window_layer);
  layer_destroy(window_layer);
	
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}