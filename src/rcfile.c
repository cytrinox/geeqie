/*
 * Geeqie
 * (C) 2006 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "gqview.h"
#include "rcfile.h"

#include "filelist.h"
#include "slideshow.h"
#include "ui_fileops.h"
#include "bar_exif.h"


/*
 *-----------------------------------------------------------------------------
 * line write/parse routines (private)
 *-----------------------------------------------------------------------------
 */ 
 
/* 
   returns text without quotes or NULL for empty or broken string
   any text up to first '"' is skipped
   tail is set to point at the char after the second '"'
   or at the ending \0 
   
*/

gchar *quoted_value(const gchar *text, const gchar **tail)
{
	const gchar *ptr;
	gint c = 0;
	gint l = strlen(text);
	gchar *retval = NULL;
	
	if (tail) *tail = text;
	
	if (l == 0) return retval;

	while (c < l && text[c] !='"') c++;
	if (text[c] == '"')
		{
		gint e;
		c++;
		ptr = text + c;
		e = c;
		while (e < l)
			{
			if (text[e-1] != '\\' && text[e] == '"') break;
			e++;
			}
		if (text[e] == '"')
			{
			if (e - c > 0)
				{
				gchar *substring = g_strndup(ptr, e - c);
				
				if (substring)
					{
					retval = g_strcompress(substring);
					g_free(substring);
					}
				}
			}
		if (tail) *tail = text + e + 1;
		}
	else
		/* for compatibility with older formats (<0.3.7)
		 * read a line without quotes too */
		{
		c = 0;
		while (c < l && text[c] !=' ' && text[c] !=8 && text[c] != '\n') c++;
		if (c != 0)
			{
			retval = g_strndup(text, c);
			}
		if (tail) *tail = text + c;
		}

	return retval;
}

gchar *escquote_value(const gchar *text)
{
	gchar *e;
	
	if (!text) return g_strdup("\"\"");

	e = g_strescape(text, "");
	if (e)
		{
		gchar *retval = g_strdup_printf("\"%s\"", e);
		g_free(e);
		return retval;
		}
	return g_strdup("\"\"");
}

static void write_char_option(FILE *f, gchar *label, gchar *text)
{
	gchar *escval = escquote_value(text);

	fprintf(f,"%s: %s\n", label, escval);
	g_free(escval);
}

static gchar *read_char_option(FILE *f, gchar *option, gchar *label, gchar *value, gchar *text)
{
	if (strcasecmp(option, label) == 0)
		{
		g_free(text);
		text = quoted_value(value, NULL);
		}
	return text;
}

static void write_color_option(FILE *f, gchar *label, GdkColor *color)
{
	if (color)
		{
		gchar *colorstring = gdk_color_to_string (color);

		write_char_option(f, label, colorstring);
		g_free(colorstring);
		}
	else
		fprintf(f,"%s: \n", label);
}

static GdkColor *read_color_option(FILE *f, gchar *option, gchar *label, gchar *value, GdkColor *color)
{
	if (strcasecmp(option, label) == 0)
		{
		gchar *colorstr = quoted_value(value, NULL);
		if (colorstr) gdk_color_parse(colorstr, color);
		g_free(colorstr);
		}
	return color;
}


static void write_int_option(FILE *f, gchar *label, gint n)
{
	fprintf(f,"%s: %d\n\n", label, n);
}

static gint read_int_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n)
{
	if (strcasecmp(option, label) == 0)
		{
		n = strtol(value, NULL, 10);
		}
	return n;
}

static void write_int_unit_option(FILE *f, gchar *label, gint n, gint subunits)
{
	gint l, r;

	if (subunits > 0)
		{
		l = n / subunits;
		r = n % subunits;
		}
	else
		{
		l = n;
		r = 0;
		}

	fprintf(f,"%s: %d.%d\n\n", label, l, r);
}

static gint read_int_unit_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n, gint subunits)
{
	if (strcasecmp(option, label) == 0)
		{
		gint l, r;
		gchar *ptr;

		ptr = value;
		while (*ptr != '\0' && *ptr != '.') ptr++;
		if (*ptr == '.')
			{
			*ptr = '\0';
			l = strtol(value, NULL, 10);
			*ptr = '.';
			ptr++;
			r = strtol(ptr, NULL, 10);
			}
		else
			{
			l = strtol(value, NULL, 10);
			r = 0;
			}

		n = l * subunits + r;
		}
	return n;
}

static void write_bool_option(FILE *f, gchar *label, gint n)
{
	fprintf(f,"%s: ", label);
	if (n) fprintf(f,"true\n"); else fprintf(f,"false\n");
	fprintf(f,"\n");
}

static gint read_bool_option(FILE *f, gchar *option, gchar *label, gchar *value, gint n)
{
	if (strcasecmp(option, label) == 0)
		{
		if (strcasecmp(value, "true") == 0)
			n = TRUE;
		else
			n = FALSE;
		}
	return n;
}

/*
 *-----------------------------------------------------------------------------
 * save configuration (public)
 *-----------------------------------------------------------------------------
 */ 

void save_options(void)
{
	FILE *f;
	gchar *rc_path;
	gchar *rc_pathl;
	gint i;

	rc_path = g_strconcat(homedir(), "/", GQVIEW_RC_DIR, "/", RC_FILE_NAME, NULL);

	rc_pathl = path_from_utf8(rc_path);
	f = fopen(rc_pathl, "w");
	g_free(rc_pathl);
	if (!f)
		{
		gchar *buf;

		buf = g_strdup_printf(_("error saving config file: %s\n"), rc_path);
		print_term(buf);
		g_free(buf);

		g_free(rc_path);
		return;
		}

	fprintf(f,"######################################################################\n");
	fprintf(f,"#                         Geeqie config file         version %7s #\n", VERSION);
	fprintf(f,"######################################################################\n");
	fprintf(f,"\n");
	fprintf(f,"# Note: This file is autogenerated. Options can be changed here,\n");
	fprintf(f,"#       but user comments and formatting will be lost.\n");
	fprintf(f,"\n");
	fprintf(f,"##### General Options #####\n\n");

	write_int_option(f, "layout_style", layout_style);
	write_char_option(f, "layout_order", layout_order);
	fprintf(f,"\n");
	write_bool_option(f, "layout_view_as_icons", layout_view_icons);
	write_bool_option(f, "layout_view_as_tree", layout_view_tree);
	write_bool_option(f, "show_icon_names", show_icon_names);
	fprintf(f,"\n");

	write_bool_option(f, "tree_descend_folders", tree_descend_subdirs);
	write_bool_option(f, "lazy_image_sync", lazy_image_sync);
	write_bool_option(f, "update_on_time_change", update_on_time_change);
	write_bool_option(f, "exif_auto_rotate", exif_rotate_enable);
	fprintf(f,"\n");

	write_bool_option(f, "enable_startup_path", startup_path_enable);
	write_char_option(f, "startup_path", startup_path);
	fprintf(f,"\n");

	fprintf(f,"zoom_mode: ");
	if (zoom_mode == ZOOM_RESET_ORIGINAL) fprintf(f,"original\n");
	if (zoom_mode == ZOOM_RESET_FIT_WINDOW) fprintf(f,"fit\n");
	if (zoom_mode == ZOOM_RESET_NONE) fprintf(f,"dont_change\n");

	write_bool_option(f, "two_pass_scaling", two_pass_zoom);

	write_bool_option(f, "zoom_to_fit_allow_expand", zoom_to_fit_expands);
	fprintf(f,"\n");

	write_bool_option(f, "fit_window_to_image", fit_window);
	write_bool_option(f, "limit_window_size", limit_window_size);
	write_int_option(f, "max_window_size", max_window_size);
	write_bool_option(f, "limit_autofit_size", limit_autofit_size);
	write_int_option(f, "max_autofit_size", max_autofit_size);
	fprintf(f,"\n");

	write_bool_option(f, "progressive_keyboard_scrolling", progressive_key_scrolling);
	write_int_option(f, "scroll_reset_method", scroll_reset_method);
	fprintf(f,"\n");

	write_bool_option(f, "enable_thumbnails", thumbnails_enabled);
	write_int_option(f, "thumbnail_width", thumb_max_width);
	write_int_option(f, "thumbnail_height", thumb_max_height);
	write_bool_option(f, "cache_thumbnails", enable_thumb_caching);
	write_bool_option(f, "cache_thumbnails_into_dirs", enable_thumb_dirs);
	write_bool_option(f, "thumbnail_fast", thumbnail_fast);
	write_bool_option(f, "use_xvpics_thumbnails", use_xvpics_thumbnails);
	write_bool_option(f, "thumbnail_spec_standard", thumbnail_spec_standard);
	fprintf(f,"\n");

	write_bool_option(f, "local_metadata", enable_metadata_dirs);
	fprintf(f,"\n");

	write_int_option(f, "sort_method", (gint)file_sort_method);
	write_bool_option(f, "sort_ascending", file_sort_ascending);
	write_bool_option(f, "sort_case_sensitive", file_sort_case_sensitive);
	fprintf(f,"\n");

	write_bool_option(f, "confirm_delete", confirm_delete);
	write_bool_option(f, "enable_delete_key", enable_delete_key);
	write_bool_option(f, "safe_delete", safe_delete_enable);
	write_char_option(f, "safe_delete_path", safe_delete_path);
	write_int_option(f, "safe_delete_size", safe_delete_size);
	fprintf(f,"\n");
	write_bool_option(f, "tools_float", tools_float);
	write_bool_option(f, "tools_hidden", tools_hidden);
	write_bool_option(f, "restore_tool_state", restore_tool);

	write_bool_option(f, "toolbar_hidden", toolbar_hidden);

	write_bool_option(f, "mouse_wheel_scrolls", mousewheel_scrolls);
	write_bool_option(f, "in_place_rename", enable_in_place_rename);

	write_int_option(f, "open_recent_max", recent_list_max);

	write_int_option(f, "image_cache_size_max", tile_cache_max);

	write_int_option(f, "thumbnail_quality", thumbnail_quality);
	write_int_option(f, "zoom_quality", zoom_quality);
	write_int_option(f, "dither_quality", dither_quality);

	write_int_option(f, "zoom_increment", zoom_increment);

	write_bool_option(f, "enable_read_ahead", enable_read_ahead);

	write_bool_option(f, "display_dialogs_under_mouse", place_dialogs_under_mouse);

	write_bool_option(f, "user_specified_window_background", user_specified_window_background);
	write_color_option(f, "window_background_color", &window_background_color);

	write_int_option(f, "fullscreen_screen", fullscreen_screen);
	write_bool_option(f, "fullscreen_clean_flip", fullscreen_clean_flip);
	write_bool_option(f, "fullscreen_disable_saver", fullscreen_disable_saver);
	write_bool_option(f, "fullscreen_above", fullscreen_above);
	write_bool_option(f, "show_fullscreen_info", show_fullscreen_info);
	write_char_option(f, "fullscreen_info", fullscreen_info);

	write_int_option(f, "custom_similarity_threshold", dupe_custom_threshold);

	fprintf(f,"\n##### Slideshow Options #####\n\n");

	write_int_unit_option(f, "slideshow_delay", slideshow_delay, SLIDESHOW_SUBSECOND_PRECISION);

	write_bool_option(f, "slideshow_random", slideshow_random);
	write_bool_option(f, "slideshow_repeat", slideshow_repeat);

	fprintf(f,"\n##### Filtering Options #####\n\n");

	write_bool_option(f, "show_dotfiles", show_dot_files);
	write_bool_option(f, "disable_filtering", file_filter_disable);
	filter_write_list(f);
	
	sidecar_ext_write(f);

	fprintf(f,"\n##### Color Profiles #####\n\n");

#ifndef HAVE_LCMS
	fprintf(f,"# NOTICE: Geeqie was not built with support for color profiles,\n"
		  "#         color profile options will have no effect.\n\n");
#endif

	write_bool_option(f, "color_profile_enabled", color_profile_enabled);
	write_bool_option(f, "color_profile_use_image", color_profile_use_image);
	write_int_option(f, "color_profile_input_type", color_profile_input_type);
	for (i = 0; i < COLOR_PROFILE_INPUTS; i++)
		{
		gchar *buf;

		buf = g_strdup_printf("color_profile_input_file_%d", i + 1);
		write_char_option(f, buf, color_profile_input_file[i]);
		g_free(buf);

		buf = g_strdup_printf("color_profile_input_name_%d", i + 1);
		write_char_option(f, buf, color_profile_input_name[i]);
		g_free(buf);
		}
	fprintf(f,"\n");
	write_int_option(f, "color_profile_screen_type", color_profile_screen_type);
	write_char_option(f, "color_profile_screen_file_1", color_profile_screen_file);

	fprintf(f,"\n##### External Programs #####\n");
	fprintf(f,"# Maximum of 10 programs (external_1 through external_10)\n");
	fprintf(f,"# format: external_n: \"menu name\" \"command line\"\n\n");

	for (i = 0; i < GQVIEW_EDITOR_SLOTS; i++)
		{
		char *qname = escquote_value(editor_name[i]);
		char *qcommand = escquote_value(editor_command[i]);
		fprintf(f,"external_%d: %s %s\n", i+1, qname, qcommand);
		g_free(qname);
		g_free(qcommand);
		}

	fprintf(f,"\n##### Collection Options #####\n\n");

	write_bool_option(f, "rectangular_selections", collection_rectangular_selection);

	fprintf(f,"\n##### Window Positions #####\n\n");

	write_bool_option(f, "restore_window_positions", save_window_positions);
	fprintf(f,"\n");
	write_int_option(f, "main_window_x", main_window_x);
	write_int_option(f, "main_window_y", main_window_y);
	write_int_option(f, "main_window_width", main_window_w);
	write_int_option(f, "main_window_height", main_window_h);
	write_bool_option(f, "main_window_maximized", main_window_maximized);
	write_int_option(f, "float_window_x", float_window_x);
	write_int_option(f, "float_window_y", float_window_y);
	write_int_option(f, "float_window_width", float_window_w);
	write_int_option(f, "float_window_height", float_window_h);
	write_int_option(f, "float_window_divider", float_window_divider);
	write_int_option(f, "divider_position_h", window_hdivider_pos);
	write_int_option(f, "divider_position_v", window_vdivider_pos);

	fprintf(f,"\n##### Exif #####\n# 0: never\n# 1: if set\n# 2: always\n");
	for (i = 0; ExifUIList[i].key; i++)
		{
		fprintf(f,"exif_");
		write_int_option(f, (gchar *)ExifUIList[i].key, ExifUIList[i].current);
		}

	fprintf(f,"######################################################################\n");
	fprintf(f,"#                      end of Geeqie config file                     #\n");
	fprintf(f,"######################################################################\n");

	fclose(f);

	g_free(rc_path);
}

/*
 *-----------------------------------------------------------------------------
 * load configuration (public)
 *-----------------------------------------------------------------------------
 */ 

void load_options(void)
{
	FILE *f;
	gchar *rc_path;
	gchar *rc_pathl;
	gchar s_buf[1024];
	gchar *s_buf_ptr;
	gchar option[1024];
	gchar value[1024];
	gchar value_all[1024];
	gint c,l,i;

	for (i = 0; ExifUIList[i].key; i++)
		ExifUIList[i].current = ExifUIList[i].default_value;

	rc_path = g_strconcat(homedir(), "/", GQVIEW_RC_DIR, "/", RC_FILE_NAME, NULL);

	rc_pathl = path_from_utf8(rc_path);
	f = fopen(rc_pathl,"r");
	g_free(rc_pathl);
	if (!f)
		{
		g_free(rc_path);
		return;
		}

	while (fgets(s_buf,1024,f))
		{
		if (s_buf[0]=='#') continue;
		if (s_buf[0]=='\n') continue;
		c = 0;
		l = strlen(s_buf);
		while (s_buf[c] != ':' && c < l) c++;
		if (c >= l) continue;
		s_buf[c] = '\0';
		c++;
		while ((s_buf[c] == ' ' || s_buf[c] == 8) && c < l) c++;
		s_buf_ptr = s_buf + c;
		strncpy(value_all, s_buf_ptr, sizeof(value_all));
		while (s_buf[c] != 8 && s_buf[c] != ' ' && s_buf[c] != '\n' && c < l) c++;
		s_buf[c] = '\0';
		strncpy(option, s_buf, sizeof(option));
		strncpy(value, s_buf_ptr, sizeof(value));

		/* general options */

		layout_style = read_int_option(f, option,
			"layout_style", value, layout_style);
		layout_order = read_char_option(f, option,
			"layout_order", value, layout_order);
		layout_view_icons = read_bool_option(f, option,
			"layout_view_as_icons", value, layout_view_icons);
		layout_view_tree = read_bool_option(f, option,
			"layout_view_as_tree", value, layout_view_tree);
		show_icon_names = read_bool_option(f, option,
			"show_icon_names", value, show_icon_names);

		tree_descend_subdirs = read_bool_option(f, option,
			"tree_descend_folders", value, tree_descend_subdirs);
		lazy_image_sync = read_bool_option(f, option,
			"lazy_image_sync", value, lazy_image_sync);
		update_on_time_change = read_bool_option(f, option,
			"update_on_time_change", value, update_on_time_change);
		exif_rotate_enable = read_bool_option(f, option,
			"exif_auto_rotate", value, exif_rotate_enable);

		startup_path_enable = read_bool_option(f, option,
			"enable_startup_path", value, startup_path_enable);
		startup_path = read_char_option(f, option,
			"startup_path", value_all, startup_path);

		if (strcasecmp(option,"zoom_mode") == 0)
                        {
                        if (strcasecmp(value, "original") == 0) zoom_mode = ZOOM_RESET_ORIGINAL;
                        if (strcasecmp(value, "fit") == 0) zoom_mode = ZOOM_RESET_FIT_WINDOW;
                        if (strcasecmp(value, "dont_change") == 0) zoom_mode = ZOOM_RESET_NONE;
                        }
		two_pass_zoom = read_bool_option(f, option,
			"two_pass_scaling", value, two_pass_zoom);
		zoom_to_fit_expands = read_bool_option(f, option,
			"zoom_to_fit_allow_expand", value, zoom_to_fit_expands);

		fit_window = read_bool_option(f, option,
			"fit_window_to_image", value, fit_window);
		limit_window_size = read_bool_option(f, option,
			"limit_window_size", value, limit_window_size);
		max_window_size = read_int_option(f, option,
			"max_window_size", value, max_window_size);
		limit_autofit_size = read_bool_option(f, option,
			"limit_autofit_size", value, limit_autofit_size);
		max_autofit_size = read_int_option(f, option,
			"max_autofit_size", value, max_autofit_size);
		progressive_key_scrolling = read_bool_option(f, option,
			"progressive_keyboard_scrolling", value, progressive_key_scrolling);
		scroll_reset_method = read_int_option(f, option,
			"scroll_reset_method", value, scroll_reset_method);

		thumbnails_enabled = read_bool_option(f, option,
			"enable_thumbnails", value, thumbnails_enabled);
		thumb_max_width = read_int_option(f, option,
			"thumbnail_width", value, thumb_max_width);
		if (thumb_max_width < 16) thumb_max_width = 16;
		thumb_max_height = read_int_option(f, option,
			"thumbnail_height", value, thumb_max_height);
		if (thumb_max_height < 16) thumb_max_height = 16;
		enable_thumb_caching = read_bool_option(f, option,
			"cache_thumbnails", value, enable_thumb_caching);
		enable_thumb_dirs = read_bool_option(f, option,
			"cache_thumbnails_into_dirs", value, enable_thumb_dirs);
		thumbnail_fast = read_bool_option(f, option,
			"thumbnail_fast", value, thumbnail_fast);
		use_xvpics_thumbnails = read_bool_option(f, option,
			"use_xvpics_thumbnails", value, use_xvpics_thumbnails);
		thumbnail_spec_standard = read_bool_option(f, option,
			"thumbnail_spec_standard", value, thumbnail_spec_standard);

		enable_metadata_dirs = read_bool_option(f, option,
			"local_metadata", value, enable_metadata_dirs);

		file_sort_method = (SortType)read_int_option(f, option,
			"sort_method", value, (gint)file_sort_method);
		file_sort_ascending = read_bool_option(f, option,
			"sort_ascending", value, file_sort_ascending);
		file_sort_case_sensitive = read_bool_option(f, option,
			"sort_case_sensitive", value, file_sort_case_sensitive);

		confirm_delete = read_bool_option(f, option,
			"confirm_delete", value, confirm_delete);
		enable_delete_key = read_bool_option(f, option,
			"enable_delete_key", value, enable_delete_key);
		safe_delete_enable = read_bool_option(f, option,
			"safe_delete",  value, safe_delete_enable);
		safe_delete_path = read_char_option(f, option,
			"safe_delete_path", value, safe_delete_path);
		safe_delete_size = read_int_option(f, option,
			"safe_delete_size", value, safe_delete_size);

		tools_float = read_bool_option(f, option,
			"tools_float", value, tools_float);
		tools_hidden = read_bool_option(f, option,
			"tools_hidden", value, tools_hidden);
		restore_tool = read_bool_option(f, option,
			"restore_tool_state", value, restore_tool);

		toolbar_hidden = read_bool_option(f, option,
			"toolbar_hidden", value, toolbar_hidden);

		mousewheel_scrolls = read_bool_option(f, option,
			"mouse_wheel_scrolls", value, mousewheel_scrolls);
		enable_in_place_rename = read_bool_option(f, option,
			"in_place_rename", value, enable_in_place_rename);

		recent_list_max = read_int_option(f, option,
			"open_recent_max", value, recent_list_max);

		tile_cache_max = read_int_option(f, option,
			"image_cache_size_max", value, tile_cache_max);

		thumbnail_quality = CLAMP(read_int_option(f, option,
			"thumbnail_quality", value, thumbnail_quality), GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		zoom_quality = CLAMP(read_int_option(f, option,
			"zoom_quality", value, zoom_quality), GDK_INTERP_NEAREST, GDK_INTERP_HYPER);
		dither_quality = CLAMP(read_int_option(f, option,
			"dither_quality", value, dither_quality), GDK_RGB_DITHER_NONE, GDK_RGB_DITHER_MAX);

		zoom_increment = read_int_option(f, option,
			"zoom_increment", value, zoom_increment);

		enable_read_ahead = read_bool_option(f, option,
			"enable_read_ahead", value, enable_read_ahead);

		place_dialogs_under_mouse = read_bool_option(f, option,
			"display_dialogs_under_mouse", value, place_dialogs_under_mouse);

		user_specified_window_background = read_bool_option(f, option,
			"user_specified_window_background", value, user_specified_window_background);
		read_color_option(f, option,
			"window_background_color", value, &window_background_color);

		fullscreen_screen = read_int_option(f, option,
			"fullscreen_screen", value, fullscreen_screen);
		fullscreen_clean_flip = read_bool_option(f, option,
			"fullscreen_clean_flip", value, fullscreen_clean_flip);
		fullscreen_disable_saver = read_bool_option(f, option,
			"fullscreen_disable_saver", value, fullscreen_disable_saver);
		fullscreen_above = read_bool_option(f, option,
			"fullscreen_above", value, fullscreen_above);
		show_fullscreen_info = read_bool_option(f, option,
			"show_fullscreen_info", value, show_fullscreen_info);
		fullscreen_info = read_char_option(f, option,
			"fullscreen_info", value_all, fullscreen_info);

		dupe_custom_threshold = read_int_option(f, option,
			"custom_similarity_threshold", value, dupe_custom_threshold);

		/* slideshow options */

		slideshow_delay = read_int_unit_option(f, option,
			"slideshow_delay", value, slideshow_delay, SLIDESHOW_SUBSECOND_PRECISION);
		slideshow_random = read_bool_option(f, option,
			"slideshow_random", value, slideshow_random);
		slideshow_repeat = read_bool_option(f, option,
			"slideshow_repeat", value, slideshow_repeat);

		/* filtering options */

		show_dot_files = read_bool_option(f, option,
			"show_dotfiles", value, show_dot_files);
		file_filter_disable = read_bool_option(f, option,
			"disable_filtering", value, file_filter_disable);

		if (strcasecmp(option, "filter_ext") == 0)
			{
			filter_parse(value_all);
			}

		if (strcasecmp(option, "sidecar_ext") == 0)
			{
			sidecar_ext_parse(value_all, TRUE);
			}
		
		/* Color Profiles */

		color_profile_enabled = read_bool_option(f, option,
			"color_profile_enabled", value, color_profile_enabled);
		color_profile_use_image = read_bool_option(f, option,
			"color_profile_use_image", value, color_profile_use_image);
		color_profile_input_type = read_int_option(f, option,
			"color_profile_input_type", value, color_profile_input_type);

		if (strncasecmp(option, "color_profile_input_file_", 25) == 0)
                        {
                        i = strtol(option + 25, NULL, 0) - 1;
			if (i >= 0 && i < COLOR_PROFILE_INPUTS)
				{
				color_profile_input_file[i] = read_char_option(f, option,
					option, value, color_profile_input_file[i]);
				}
			}
		if (strncasecmp(option, "color_profile_input_name_", 25) == 0)
                        {
                        i = strtol(option + 25, NULL, 0) - 1;
			if (i >= 0 && i < COLOR_PROFILE_INPUTS)
				{
				color_profile_input_name[i] = read_char_option(f, option,
					option, value, color_profile_input_name[i]);
				}
			}

		color_profile_screen_type = read_int_option(f, option,
			"color_profile_screen_type", value, color_profile_screen_type);
		color_profile_screen_file = read_char_option(f, option,
			"color_profile_screen_file_1", value, color_profile_screen_file);

		/* External Programs */

		if (strncasecmp(option, "external_", 9) == 0)
			{
			i = strtol(option + 9, NULL, 0);
			if (i > 0 && i <= GQVIEW_EDITOR_SLOTS)
				{
				const gchar *ptr;
				i--;
				g_free(editor_name[i]);
				g_free(editor_command[i]);
				
				editor_name[i] = quoted_value(value_all, &ptr);
				editor_command[i] = quoted_value(ptr, NULL);
				}
			}

		/* colection options */

		collection_rectangular_selection = read_bool_option(f, option,
			"rectangular_selections", value, collection_rectangular_selection);

		/* window positions */

		save_window_positions = read_bool_option(f, option,
			"restore_window_positions", value, save_window_positions);

		main_window_x = read_int_option(f, option,
			"main_window_x", value, main_window_x);
		main_window_y = read_int_option(f, option,
			"main_window_y", value, main_window_y);
		main_window_w = read_int_option(f, option,
			"main_window_width", value, main_window_w);
		main_window_h = read_int_option(f, option,
			"main_window_height", value, main_window_h);
		main_window_maximized = read_bool_option(f, option,
			"main_window_maximized", value, main_window_maximized);
		float_window_x = read_int_option(f, option,
			"float_window_x", value, float_window_x);
		float_window_y = read_int_option(f, option,
			"float_window_y", value, float_window_y);
		float_window_w = read_int_option(f, option,
			"float_window_width", value, float_window_w);
		float_window_h = read_int_option(f, option,
			"float_window_height", value, float_window_h);
		float_window_divider = read_int_option(f, option,
			"float_window_divider", value, float_window_divider);
		window_hdivider_pos = read_int_option(f, option,
			"divider_position_h", value, window_hdivider_pos);
		window_vdivider_pos = read_int_option(f, option,
			"divider_position_v", value, window_vdivider_pos);

		if (0 == strncasecmp(option, "exif_", 5))
			{
			for (i = 0; ExifUIList[i].key; i++)
				if (0 == strcasecmp(option+5, ExifUIList[i].key))
					ExifUIList[i].current = strtol(value, NULL, 10);
		  	}
		}

	fclose(f);
	g_free(rc_path);
}

