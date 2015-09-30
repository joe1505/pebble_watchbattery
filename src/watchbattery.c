/* $Id: joesdig.c,v 1.7 2015/09/20 11:40:51 jrothwei Exp jrothwei $
** Joseph Rothweiler, Sensicomm LLC. Started 27Aug2015.
** Starting to build my own digital pebble face.
** Inspired by the classio-battery-connection example.
*/

#define BACK_COLOR GColorWhite
#define FRONT_COLOR GColorBlack

#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_seconds_layer;
static TextLayer *s_time_layer;
static TextLayer *s_debug_layer;
static TextLayer *s_daydate_layer;
static TextLayer *s_battery_layer;
static TextLayer *s_connection_layer;

static int battery_prev = 130;
static int battery_current = 0;
static unsigned battery_saves[4] = {0,0,0,0};

static void handle_battery(BatteryChargeState charge_state) {
	static char battery_text[5]; // "100%" + null;


	if (charge_state.is_charging) {
		snprintf(battery_text, sizeof(battery_text), "+++");
		battery_current = 120;
	} else if (charge_state.is_plugged) {
		snprintf(battery_text, sizeof(battery_text), "###");
		battery_current = 110;
	} else {
		battery_current = charge_state.charge_percent;
		snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
	}
	text_layer_set_text(s_battery_layer, battery_text);
}

static int minute_saved = 100; // Impossible value, to force change on first call.
static int hour_saved = 100;   // Impossible value, to force change on first call.
static void handle_second_tick(struct tm* tick_time, TimeUnits units_changed) {
	// Needs to be static because it's used by the system later.
	static char s_time_text[6];  // "00:00" + null.
	static char s_seconds_text[3]; // "00" + null.
	static char s_daydate_text[11]; // "Sun Jan 01" + null.
	static char s_debug_text[36];

	// Seconds tick, so always update seconds.
	//strftime(s_seconds_text, sizeof(s_seconds_text), "%S", tick_time);
	// Only 2 digits, to just do it directly.
	s_seconds_text[0] = '0' + (tick_time->tm_sec/10);
	s_seconds_text[1] = '0' + (tick_time->tm_sec%10);
	s_seconds_text[2] = '\0';
	text_layer_set_text(s_seconds_layer, s_seconds_text);

	if(minute_saved != tick_time->tm_min) {
		minute_saved = tick_time->tm_min;
		char *time_format; // See https://gist.github.com/alokc83/5792799
		if (clock_is_24h_style()) time_format = "%H:%M";
		else                      time_format = "%l:%M";

		strftime(s_time_text, sizeof(s_time_text), time_format, tick_time);
		text_layer_set_text(s_time_layer, s_time_text);
		// text_layer_set_text(s_time_layer, "20:00"); // Biggest, for test only.

		if(hour_saved != tick_time->tm_hour) {
			hour_saved = tick_time->tm_hour;
			// Only need to update date once per day, but savings are probably minimal.
			strftime(s_daydate_text,sizeof(s_daydate_text),"%a %b %d",tick_time);
			text_layer_set_text(s_daydate_layer,s_daydate_text);
		}

		// Update the battery at this time also.
		handle_battery(battery_state_service_peek());
		if(battery_prev != battery_current) {
			int k;
			for(k=3;k>0;k--) battery_saves[k] = battery_saves[k-1];
			battery_saves[0] = 
				( (battery_current + tick_time->tm_wday) * 100 + tick_time->tm_hour) * 100 + tick_time->tm_min;
			battery_prev = battery_current;
			snprintf(s_debug_text,sizeof(s_debug_text),"%7.7d %7.7d %7.7d",
				battery_saves[0], battery_saves[1], battery_saves[2]);
			text_layer_set_text(s_debug_layer,s_debug_text);
		}
	}
}

static void handle_bluetooth(bool connected) {
	text_layer_set_text(s_connection_layer, connected ? "BT" : " ");
}

//-----------------------------------------------------------------------
// Load and unload
static void main_window_load(Window *window) {

	// Find the whole-screen layer.
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);

	s_debug_layer = text_layer_create(GRect(0, 32,  bounds.size.w, 50));
	text_layer_set_text_color(s_debug_layer, FRONT_COLOR);
	text_layer_set_background_color(s_debug_layer, GColorClear);
	text_layer_set_font(s_debug_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14)); // The smallest.
	text_layer_set_text_alignment(s_debug_layer, GTextAlignmentLeft);

	// Make a time block (full width, height comparable to the font.)
	// Height 50 seems to give 1-pixel margin below character with ROTOTO...49
	s_time_layer = text_layer_create(GRect(0, bounds.size.h-50-30, bounds.size.w-2, 50));
	text_layer_set_text_color(s_time_layer, FRONT_COLOR);
	text_layer_set_background_color(s_time_layer, GColorClear);
	// FONT_KEY_BITHAM_34_MEDIUM_NUMBERS includes ':' but not a space character.
	// FONT_KEY_BITHAM_30_BLACK - OK, but really dark.
	// FONT_KEY_GOTHIC_28_BOLD - ok, a bit small.
	text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);

	// Seconds block (at bottom right).
	// The ifdef is a hack to work around the broken40 problem.
#ifdef PBL_PLATFORM_BASALT	
	s_seconds_layer = text_layer_create(GRect(bounds.size.w-49, bounds.size.h-32, 46, 32));
#else
	s_seconds_layer = text_layer_create(GRect(bounds.size.w-48, bounds.size.h-32, 46, 32));
#endif
	text_layer_set_text_color(s_seconds_layer, FRONT_COLOR);
	text_layer_set_background_color(s_seconds_layer, GColorClear);

	text_layer_set_font(s_seconds_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
	text_layer_set_text_alignment(s_seconds_layer, GTextAlignmentLeft);

	// DayDate block (above the time).
	// Height 32 just includes the g descender for GOTHIC_28_BOLD.
	s_daydate_layer = text_layer_create(GRect(0, bounds.size.h-50-32-27, bounds.size.w-4, 32));
	text_layer_set_text_color(s_daydate_layer, FRONT_COLOR);
	text_layer_set_background_color(s_daydate_layer, GColorClear);
	text_layer_set_font(s_daydate_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(s_daydate_layer, GTextAlignmentRight);

	// Connection block.
	s_connection_layer = text_layer_create(GRect(0, 0, bounds.size.w, 28));
	text_layer_set_text_color(s_connection_layer, FRONT_COLOR);
	text_layer_set_background_color(s_connection_layer, GColorClear);
	text_layer_set_font(s_connection_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	text_layer_set_text_alignment(s_connection_layer, GTextAlignmentLeft);
	handle_bluetooth(bluetooth_connection_service_peek());

	// Battery block - top right.
	s_battery_layer = text_layer_create(GRect(bounds.size.w-60, 0, 60, 28));
	text_layer_set_text_color(s_battery_layer, FRONT_COLOR);
	text_layer_set_background_color(s_battery_layer, GColorClear);
	text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	text_layer_set_text_alignment(s_battery_layer, GTextAlignmentRight);

	// Ensures time is displayed immediately (will break if NULL tick event accessed).
	// (This is why it's a good idea to have a separate routine to do the update itself.)
	time_t now = time(NULL);
	struct tm *current_time = localtime(&now);
	handle_second_tick(current_time, SECOND_UNIT);

	tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
	battery_state_service_subscribe(handle_battery);
	bluetooth_connection_service_subscribe(handle_bluetooth);

	layer_add_child(window_layer, text_layer_get_layer(s_debug_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_seconds_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_daydate_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_connection_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
}

static void main_window_unload(Window *window) {
	tick_timer_service_unsubscribe();
	battery_state_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	text_layer_destroy(s_debug_layer);
	text_layer_destroy(s_seconds_layer);
	text_layer_destroy(s_time_layer);
	text_layer_destroy(s_daydate_layer);
	text_layer_destroy(s_connection_layer);
	text_layer_destroy(s_battery_layer);
}

//-----------------------------------------------------------------------
// init and deinit, to be used by main.
// Defines the load and unload functions.
static void init() {
	s_main_window = window_create();
	window_set_background_color(s_main_window, BACK_COLOR);
	window_set_window_handlers(s_main_window, (WindowHandlers) {
	  .load = main_window_load,
	  .unload = main_window_unload,
	});
	window_stack_push(s_main_window, true);
}

static void deinit() {
	window_destroy(s_main_window);
}

//-----------------------------------------------------------------------
int main(void) {
	init();
	app_event_loop();
	deinit();
}