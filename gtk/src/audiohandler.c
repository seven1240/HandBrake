/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * audiohandler.c
 * Copyright (C) John Stebbins 2008 <stebbins@stebbins>
 * 
 * audiohandler.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <gtk/gtk.h>
#include "hb.h"
#include "settings.h"
#include "hb-backend.h"
#include "values.h"
#include "callbacks.h"
#include "audiohandler.h"

void
ghb_adjust_audio_rate_combos(signal_user_data_t *ud)
{
	gint titleindex, audioindex, acodec, mix;
	ghb_audio_info_t ainfo;
	GtkWidget *widget;
	
	g_debug("ghb_adjust_audio_rate_combos ()");
	titleindex = ghb_settings_combo_int(ud->settings, "title");

	widget = GHB_WIDGET(ud->builder, "audio_track");
	audioindex = ghb_lookup_combo_int("audio_track", ghb_widget_value(widget));

	widget = GHB_WIDGET(ud->builder, "audio_codec");
	acodec = ghb_lookup_combo_int("audio_codec", ghb_widget_value(widget));
	widget = GHB_WIDGET(ud->builder, "audio_mix");
	mix = ghb_lookup_combo_int("audio_mix", ghb_widget_value(widget));

	if (ghb_audio_is_passthru (acodec))
	{
		ghb_set_default_bitrate_opts (ud->builder, -1);
		if (ghb_get_audio_info (&ainfo, titleindex, audioindex))
		{
			gint br = ainfo.bitrate / 1000;
			// Set the values for bitrate and samplerate to the input rates
			ghb_set_passthru_bitrate_opts (ud->builder, br);
			ghb_ui_update(ud, "audio_bitrate", ghb_int64_value(br));
			ghb_ui_update(ud, "audio_rate", ghb_int64_value(0));
			ghb_ui_update(ud, "audio_mix", ghb_int64_value(0));
		}
		else
		{
			ghb_ui_update(ud, "audio_bitrate", ghb_int64_value(384));
			ghb_ui_update(ud, "audio_rate", ghb_int64_value(0));
			ghb_ui_update(ud, "audio_mix", ghb_int64_value(0));
		}
	}
	else if (acodec == HB_ACODEC_FAAC && mix != HB_AMIXDOWN_6CH)
	{
		gint br;

		widget = GHB_WIDGET(ud->builder, "audio_bitrate");
		br = ghb_lookup_combo_int("audio_bitrate", ghb_widget_value(widget));
		if (br > 160)
			ghb_ui_update(ud, "audio_bitrate", ghb_int64_value(160));
		ghb_set_default_bitrate_opts (ud->builder, 160);
	}
	else
	{
		ghb_set_default_bitrate_opts (ud->builder, -1);
	}
}

void
ghb_set_pref_audio(gint titleindex, signal_user_data_t *ud)
{
	gint acodec_code, mix_code, track;
	gchar *source_lang;
	GtkWidget *button;
	ghb_audio_info_t ainfo;
	gint index;
	GHashTable *track_indicies;
	gint *iptr;

	GValue *pref_audio;
	GValue *audio, *acodec, *bitrate, *rate, *mix, *drc;
	gint count, ii, list_count;
	
	g_debug("set_pref_audio");
	track_indicies = g_hash_table_new(g_int_hash, g_int_equal);
	// Clear the audio list
	ghb_clear_audio_list(ud);
	// Find "best" audio based on audio preferences
	button = GHB_WIDGET (ud->builder, "audio_add");
	source_lang = ghb_settings_get_string(ud->settings, "source_audio_lang");

	pref_audio = ghb_settings_get_value(ud->settings, "pref_audio_list");

	list_count = 0;
	count = ghb_array_len(pref_audio);
	for (ii = 0; ii < count; ii++)
	{
		audio = ghb_array_get_nth(pref_audio, ii);
		acodec = ghb_settings_get_value(audio, "audio_codec");
		bitrate = ghb_settings_get_value(audio, "audio_bitrate");
		rate = ghb_settings_get_value(audio, "audio_rate");
		mix = ghb_settings_get_value(audio, "audio_mix");
		drc = ghb_settings_get_value(audio, "audio_drc");
		acodec_code = ghb_lookup_combo_int("audio_codec", acodec);
		// If there are multiple audios using the same codec, then
		// select sequential tracks for each.  This hash keeps track 
		// of the last used track for each codec.
		iptr = g_hash_table_lookup(track_indicies, &acodec_code);
		if (iptr == NULL)
			index = 0;
		else
			index = *(gint*)iptr;

		track = ghb_find_audio_track(titleindex, source_lang, index);
		// Check to see if:
		// 1. pref codec is ac3
		// 2. source codec is not ac3
		// 3. next pref is enabled
		if (ghb_get_audio_info (&ainfo, titleindex, track) && 
			ghb_audio_is_passthru (acodec_code))
		{
			if (!ghb_audio_is_passthru(ainfo.codec))
			{
				acodec_code = ghb_get_default_acodec();
				// If there's more audio to process, or we've already
				// placed one in the list, then we can skip this one
				if ((ii + 1 < count) || (list_count != 0))
				{
					// Skip this audio
					acodec_code = 0;
				}
			}
		}
		if (titleindex >= 0 && track < 0)
			acodec_code = 0;
		if (acodec_code != 0)
		{
			// Add to audio list
			g_signal_emit_by_name(button, "clicked", ud);
			list_count++;
			ghb_ui_update(ud, "audio_track", ghb_int64_value(track));
			ghb_ui_update(ud, "audio_codec", acodec);
			if (!ghb_audio_is_passthru (acodec_code))
			{
				// This gets set autimatically if the codec is passthru
				ghb_ui_update(ud, "audio_bitrate", bitrate);
				ghb_ui_update(ud, "audio_rate", rate);
				mix_code = ghb_lookup_combo_int("audio_mix", mix);
				mix_code = ghb_get_best_mix(
					titleindex, track, acodec_code, mix_code);
				ghb_ui_update(ud, "audio_mix", ghb_int64_value(mix_code));
			}
			ghb_ui_update(ud, "audio_drc", drc);
			index++;
			g_hash_table_insert(track_indicies, &acodec_code, &index);
		}
	}
	g_free(source_lang);
	g_hash_table_destroy(track_indicies);
}

static GValue*
get_selected_asettings(signal_user_data_t *ud)
{
	GtkTreeView *treeview;
	GtkTreePath *treepath;
	GtkTreeSelection *selection;
	GtkTreeModel *store;
	GtkTreeIter iter;
	gint *indices;
	gint row;
	GValue *asettings = NULL;
	const GValue *audio_list;
	
	g_debug("get_selected_asettings ()");
	treeview = GTK_TREE_VIEW(GHB_WIDGET(ud->builder, "audio_list"));
	selection = gtk_tree_view_get_selection (treeview);
	if (gtk_tree_selection_get_selected(selection, &store, &iter))
	{
		// Get the row number
		treepath = gtk_tree_model_get_path (store, &iter);
		indices = gtk_tree_path_get_indices (treepath);
		row = indices[0];
		gtk_tree_path_free(treepath);
		// find audio settings
		if (row < 0) return NULL;
		audio_list = ghb_settings_get_value(ud->settings, "audio_list");
		if (row >= ghb_array_len(audio_list))
			return NULL;
		asettings = ghb_array_get_nth(audio_list, row);
	}
	return asettings;
}

static void
audio_list_refresh_selected(signal_user_data_t *ud)
{
	GtkTreeView *treeview;
	GtkTreePath *treepath;
	GtkTreeSelection *selection;
	GtkTreeModel *store;
	GtkTreeIter iter;
	gint *indices;
	gint row;
	GValue *asettings = NULL;
	const GValue *audio_list;
	
	g_debug("audio_list_refresh_selected ()");
	treeview = GTK_TREE_VIEW(GHB_WIDGET(ud->builder, "audio_list"));
	selection = gtk_tree_view_get_selection (treeview);
	if (gtk_tree_selection_get_selected(selection, &store, &iter))
	{
		const gchar *track, *codec, *br, *sr, *mix;
		gchar *drc, *s_track, *s_codec, *s_br, *s_sr, *s_mix;
		gdouble s_drc;
		// Get the row number
		treepath = gtk_tree_model_get_path (store, &iter);
		indices = gtk_tree_path_get_indices (treepath);
		row = indices[0];
		gtk_tree_path_free(treepath);
		// find audio settings
		if (row < 0) return;
		audio_list = ghb_settings_get_value(ud->settings, "audio_list");
		if (row >= ghb_array_len(audio_list))
			return;
		asettings = ghb_array_get_nth(audio_list, row);

		track = ghb_settings_combo_option(asettings, "audio_track");
		codec = ghb_settings_combo_option(asettings, "audio_codec");
		br = ghb_settings_combo_option(asettings, "audio_bitrate");
		sr = ghb_settings_combo_option(asettings, "audio_rate");
		mix = ghb_settings_combo_option(asettings, "audio_mix");
		drc = ghb_settings_get_string(asettings, "audio_drc");

		s_track = ghb_settings_get_string(asettings, "audio_track");
		s_codec = ghb_settings_get_string(asettings, "audio_codec");
		s_br = ghb_settings_get_string(asettings, "audio_bitrate");
		s_sr = ghb_settings_get_string(asettings, "audio_rate");
		s_mix = ghb_settings_get_string(asettings, "audio_mix");
		s_drc = ghb_settings_get_double(asettings, "audio_drc");

		gtk_list_store_set(GTK_LIST_STORE(store), &iter, 
			// These are displayed in list
			0, track,
			1, codec,
			2, br,
			3, sr,
			4, mix,
			// These are used to set combo values when a list item is selected
			5, drc,
			6, s_track,
			7, s_codec,
			8, s_br,
			9, s_sr,
			10, s_mix,
			11, s_drc,
			-1);
		g_free(drc);
		g_free(s_track);
		g_free(s_codec);
		g_free(s_br);
		g_free(s_sr);
		g_free(s_mix);
	}
}

void
audio_codec_changed_cb(GtkWidget *widget, signal_user_data_t *ud)
{
	static gint prev_acodec = 0;
	gint acodec_code, mix_code;
	GValue *asettings;
	
	g_debug("audio_codec_changed_cb ()");
	acodec_code = ghb_lookup_combo_int("audio_codec", ghb_widget_value(widget));
	if (ghb_audio_is_passthru (prev_acodec) && 
		!ghb_audio_is_passthru (acodec_code))
	{
		// Transition from passthru to not, put some audio settings back to 
		// pref settings
		gint titleindex;
		gint track;

		titleindex = ghb_settings_combo_int(ud->settings, "title");
		track = ghb_settings_combo_int(ud->settings, "audio_track");

		ghb_ui_update(ud, "audio_bitrate", ghb_string_value("160"));
		ghb_ui_update(ud, "audio_rate", ghb_string_value("source"));
		mix_code = ghb_lookup_combo_int("audio_mix", ghb_string_value("dpl2"));
		mix_code = ghb_get_best_mix( titleindex, track, acodec_code, mix_code);
		ghb_ui_update(ud, "audio_mix", ghb_int64_value(mix_code));
		ghb_ui_update(ud, "audio_drc", ghb_double_value(1.0));
	}
	ghb_adjust_audio_rate_combos(ud);
	ghb_grey_combo_options (ud->builder);
	ghb_check_dependency(ud, widget);
	prev_acodec = acodec_code;
	asettings = get_selected_asettings(ud);
	if (asettings != NULL)
	{
		ghb_widget_to_setting(asettings, widget);
		audio_list_refresh_selected(ud);
	}

	const GValue *audio_list;
	audio_list = ghb_settings_get_value(ud->settings, "audio_list");
	if (ghb_ac3_in_audio_list (audio_list))
	{
		gchar *container;

		container = ghb_settings_get_string(ud->settings, "container");
		if (strcmp(container, "mp4") == 0)
		{
			ghb_ui_update(ud, "container", ghb_string_value("m4v"));
		}
		g_free(container);
	}
}

void
audio_track_changed_cb(GtkWidget *widget, signal_user_data_t *ud)
{
	GValue *asettings;

	g_debug("audio_track_changed_cb ()");
	ghb_adjust_audio_rate_combos(ud);
	ghb_check_dependency(ud, widget);
	ghb_grey_combo_options(ud->builder);
	asettings = get_selected_asettings(ud);
	if (asettings != NULL)
	{
		const gchar *track;

		ghb_widget_to_setting(asettings, widget);
		audio_list_refresh_selected(ud);
		track = ghb_settings_combo_option(asettings, "audio_track");
		ghb_settings_set_string(asettings, "audio_track_long", track);
	}
}

void
audio_mix_changed_cb(GtkWidget *widget, signal_user_data_t *ud)
{
	GValue *asettings;

	g_debug("audio_widget_changed_cb ()");
	ghb_adjust_audio_rate_combos(ud);
	ghb_check_dependency(ud, widget);
	asettings = get_selected_asettings(ud);
	if (asettings != NULL)
	{
		ghb_widget_to_setting(asettings, widget);
		audio_list_refresh_selected(ud);
	}
}

void
audio_widget_changed_cb(GtkWidget *widget, signal_user_data_t *ud)
{
	GValue *asettings;

	g_debug("audio_widget_changed_cb ()");
	ghb_check_dependency(ud, widget);
	asettings = get_selected_asettings(ud);
	if (asettings != NULL)
	{
		ghb_widget_to_setting(asettings, widget);
		audio_list_refresh_selected(ud);
	}
}

// subtitles differ from other settings in that
// the selection is updated automaitcally when the title
// changes.  I don't want the preset selection changed as
// would happen for regular settings.
void
subtitle_changed_cb(GtkWidget *widget, signal_user_data_t *ud)
{
	const gchar *name = gtk_widget_get_name(widget);
	g_debug("subtitle_changed_cb () %s", name);
	ghb_widget_to_setting(ud->settings, widget);
	ghb_check_dependency(ud, widget);
}

void
ghb_clear_audio_list(signal_user_data_t *ud)
{
	GtkTreeView *treeview;
	GtkListStore *store;
	GValue *audio_list;
	
	g_debug("clear_audio_list ()");
	audio_list = ghb_settings_get_value(ud->settings, "audio_list");
	if (audio_list == NULL)
	{
		audio_list = ghb_array_value_new(8);
		ghb_settings_set_value(ud->settings, "audio_list", audio_list);
	}
	else
		ghb_array_value_reset(audio_list, 8);
	treeview = GTK_TREE_VIEW(GHB_WIDGET(ud->builder, "audio_list"));
	store = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	gtk_list_store_clear (store);
}

static void
add_to_audio_list(signal_user_data_t *ud, GValue *settings)
{
	GtkTreeView *treeview;
	GtkTreeIter iter;
	GtkListStore *store;
	GtkTreeSelection *selection;
	const gchar *track, *codec, *br, *sr, *mix;
	gchar *drc, *s_track, *s_codec, *s_br, *s_sr, *s_mix;
	gdouble s_drc;
	
	g_debug("add_to_audio_list ()");
	treeview = GTK_TREE_VIEW(GHB_WIDGET(ud->builder, "audio_list"));
	selection = gtk_tree_view_get_selection (treeview);
	store = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));

	track = ghb_settings_combo_option(settings, "audio_track");
	codec = ghb_settings_combo_option(settings, "audio_codec");
	br = ghb_settings_combo_option(settings, "audio_bitrate");
	sr = ghb_settings_combo_option(settings, "audio_rate");
	mix = ghb_settings_combo_option(settings, "audio_mix");
	drc = ghb_settings_get_string(settings, "audio_drc");

	s_track = ghb_settings_get_string(settings, "audio_track");
	s_codec = ghb_settings_get_string(settings, "audio_codec");
	s_br = ghb_settings_get_string(settings, "audio_bitrate");
	s_sr = ghb_settings_get_string(settings, "audio_rate");
	s_mix = ghb_settings_get_string(settings, "audio_mix");
	s_drc = ghb_settings_get_double(settings, "audio_drc");

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 
		// These are displayed in list
		0, track,
		1, codec,
		2, br,
		3, sr,
		4, mix,
		// These are used to set combo box values when a list item is selected
		5, drc,
		6, s_track,
		7, s_codec,
		8, s_br,
		9, s_sr,
		10, s_mix,
		11, s_drc,
		-1);
	gtk_tree_selection_select_iter(selection, &iter);
	g_free(drc);
	g_free(s_track);
	g_free(s_codec);
	g_free(s_br);
	g_free(s_sr);
	g_free(s_mix);
}

void
audio_list_selection_changed_cb(GtkTreeSelection *selection, signal_user_data_t *ud)
{
	GtkTreeModel *store;
	GtkTreeIter iter;
	GtkWidget *widget;
	
	g_debug("audio_list_selection_changed_cb ()");
	if (gtk_tree_selection_get_selected(selection, &store, &iter))
	{
		const gchar *track, *codec, *bitrate, *sample_rate, *mix;
		gdouble drc;

		gtk_tree_model_get(store, &iter,
						   6, &track,
						   7, &codec,
						   8, &bitrate,
						   9, &sample_rate,
						   10, &mix,
						   11, &drc,
						   -1);
		ghb_ui_update(ud, "audio_track", ghb_string_value(track));
		ghb_ui_update(ud, "audio_codec", ghb_string_value(codec));
		ghb_ui_update(ud, "audio_bitrate", ghb_string_value(bitrate));
		ghb_ui_update(ud, "audio_rate", ghb_string_value(sample_rate));
		ghb_ui_update(ud, "audio_mix", ghb_string_value(mix));
		ghb_ui_update(ud, "audio_drc", ghb_double_value(drc));
		widget = GHB_WIDGET (ud->builder, "audio_remove");
		gtk_widget_set_sensitive(widget, TRUE);
		//widget = GHB_WIDGET (ud->builder, "audio_update");
		//gtk_widget_set_sensitive(widget, TRUE);
	}
	else
	{
		widget = GHB_WIDGET (ud->builder, "audio_remove");
		gtk_widget_set_sensitive(widget, FALSE);
		//widget = GHB_WIDGET (ud->builder, "audio_update");
		//gtk_widget_set_sensitive(widget, FALSE);
	}
}

void
audio_add_clicked_cb(GtkWidget *xwidget, signal_user_data_t *ud)
{
	// Add the current audio settings to the list.
	GValue *asettings;
	GtkWidget *widget;
	gint count;
	GValue *audio_list;
	const gchar *track;
	
	g_debug("audio_add_clicked_cb ()");
	asettings = ghb_dict_value_new();
	// Only allow up to 8 audio entries
	widget = GHB_WIDGET(ud->builder, "audio_track");
	ghb_settings_take_value(asettings, "audio_track", ghb_widget_value(widget));
	widget = GHB_WIDGET(ud->builder, "audio_codec");
	ghb_settings_take_value(asettings, "audio_codec", ghb_widget_value(widget));
	widget = GHB_WIDGET(ud->builder, "audio_bitrate");
	ghb_settings_take_value(asettings, "audio_bitrate", ghb_widget_value(widget));
	widget = GHB_WIDGET(ud->builder, "audio_rate");
	ghb_settings_take_value(asettings, "audio_rate", ghb_widget_value(widget));
	widget = GHB_WIDGET(ud->builder, "audio_mix");
	ghb_settings_take_value(asettings, "audio_mix", ghb_widget_value(widget));
	widget = GHB_WIDGET(ud->builder, "audio_drc");
	ghb_settings_take_value(asettings, "audio_drc", ghb_widget_value(widget));
	track = ghb_settings_combo_option(asettings, "audio_track");
	ghb_settings_set_string(asettings, "audio_track_long", track);

	audio_list = ghb_settings_get_value(ud->settings, "audio_list");
	if (audio_list == NULL)
	{
		audio_list = ghb_array_value_new(8);
		ghb_settings_set_value(ud->settings, "audio_list", audio_list);
	}
	ghb_array_append(audio_list, asettings);
	add_to_audio_list(ud, asettings);
	count = ghb_array_len(audio_list);
	if (count >= 8)
	{
		gtk_widget_set_sensitive(xwidget, FALSE);
	}
}

void
audio_remove_clicked_cb(GtkWidget *widget, signal_user_data_t *ud)
{
	GtkTreeView *treeview;
	GtkTreePath *treepath;
	GtkTreeSelection *selection;
	GtkTreeModel *store;
	GtkTreeIter iter, nextIter;
	gint *indices;
	gint row;
	GValue *audio_list;

	g_debug("audio_remove_clicked_cb ()");
	treeview = GTK_TREE_VIEW(GHB_WIDGET(ud->builder, "audio_list"));
	selection = gtk_tree_view_get_selection (treeview);
	if (gtk_tree_selection_get_selected(selection, &store, &iter))
	{
		nextIter = iter;
		if (!gtk_tree_model_iter_next(store, &nextIter))
		{
			nextIter = iter;
			if (gtk_tree_model_get_iter_first(store, &nextIter))
			{
				gtk_tree_selection_select_iter (selection, &nextIter);
			}
		}
		else
		{
			gtk_tree_selection_select_iter (selection, &nextIter);
		}
		// Get the row number
		treepath = gtk_tree_model_get_path (store, &iter);
		indices = gtk_tree_path_get_indices (treepath);
		row = indices[0];
		gtk_tree_path_free(treepath);
		// Remove the selected item
		gtk_list_store_remove (GTK_LIST_STORE(store), &iter);
		// remove from audio settings list
		if (row < 0) return;
		widget = GHB_WIDGET (ud->builder, "audio_add");
		gtk_widget_set_sensitive(widget, TRUE);
		audio_list = ghb_settings_get_value(ud->settings, "audio_list");
		if (row >= ghb_array_len(audio_list))
			return;
		GValue *old = ghb_array_get_nth(audio_list, row);
		ghb_value_free(old);
		ghb_array_remove(audio_list, row);
	}
}
