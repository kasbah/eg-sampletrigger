/*
  LV2 Sampler Example Plugin UI
  Copyright 2011-2012 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/**
   @file ui.c Sampler Plugin UI
*/

#include <stdlib.h>

#include <gtk/gtk.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/patch/patch.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include "./uris.h"

#define SAMPLER_UI_URI "http://github.com/kasbah/eg-sampletrigger#ui"

typedef struct {
	LV2_Atom_Forge forge;

	LV2_URID_Map* map;
	SamplerURIs   uris;

	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	GtkWidget* box;
	GtkWidget* button;
	GtkWidget* button2;
	GtkWidget* label;
} SamplerUI;

typedef struct LV2_Atom_MidiEvent {
    LV2_Atom_Event event;
    uint8_t data[3];
} LV2_Atom_MidiEvent;

static void
on_trig_clicked(GtkWidget* widget,
                void*      handle)
{
	SamplerUI* ui = (SamplerUI*)handle;

    /* the data that we want to send */
    uint8_t raw_midi[3];
    raw_midi[0] = 0x90;
    raw_midi[1] = 127;
    raw_midi[2] = 127;
 
    /* and the length of the data */
    size_t  midi_message_length = 3;
 
    /* Message payload header */
    LV2_Atom midiatom;
    midiatom.type = ui->uris.midi_Event;
    midiatom.size = midi_message_length;
 
    /* buffer used to forge the message in */
    uint8_t atom_buf[64];
 
    /* make forge use this buffer for now */
    lv2_atom_forge_set_buffer(&ui->forge, atom_buf, sizeof(atom_buf));
 
    /* .. then we need to specify what this message is about and how long the
     * payload is: add the header */
    lv2_atom_forge_raw(&ui->forge, &midiatom, sizeof(LV2_Atom));
    /* now we we can add the actual data */
    lv2_atom_forge_raw(&ui->forge, raw_midi, midi_message_length);
    /* finally pad the message to make it 32bit aligned */
    lv2_atom_forge_pad(&ui->forge, sizeof(LV2_Atom) + midi_message_length);
 
    /* send it */
    ui->write(ui->controller, 0, lv2_atom_total_size(atom_buf), ui->uris.atom_eventTransfer, atom_buf);
}
static void
on_load_clicked(GtkWidget* widget,
                void*      handle)
{
	SamplerUI* ui = (SamplerUI*)handle;

	/* Create a dialog to select a sample file. */
	GtkWidget* dialog = gtk_file_chooser_dialog_new(
		"Load Sample",
		NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		NULL);

	/* Run the dialog, and return if it is cancelled. */
	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(dialog);
		return;
	}

	/* Get the file path from the dialog. */
	char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

	/* Got what we need, destroy the dialog. */
	gtk_widget_destroy(dialog);

#define OBJ_BUF_SIZE 1024
	uint8_t obj_buf[OBJ_BUF_SIZE];
	lv2_atom_forge_set_buffer(&ui->forge, obj_buf, OBJ_BUF_SIZE);

	LV2_Atom* msg = write_set_file(&ui->forge, &ui->uris,
	                               filename, strlen(filename));

	ui->write(ui->controller, 0, lv2_atom_total_size(msg),
	          ui->uris.atom_eventTransfer,
	          msg);

	g_free(filename);
}

static LV2UI_Handle
instantiate(const LV2UI_Descriptor*   descriptor,
            const char*               plugin_uri,
            const char*               bundle_path,
            LV2UI_Write_Function      write_function,
            LV2UI_Controller          controller,
            LV2UI_Widget*             widget,
            const LV2_Feature* const* features)
{
	SamplerUI* ui = (SamplerUI*)malloc(sizeof(SamplerUI));
	ui->map        = NULL;
	ui->write      = write_function;
	ui->controller = controller;
	ui->box        = NULL;
    ui->button     = NULL;
	ui->button2    = NULL;
	ui->label      = NULL;

	*widget = NULL;

	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID_URI "#map")) {
			ui->map = (LV2_URID_Map*)features[i]->data;
		}
	}

	if (!ui->map) {
		fprintf(stderr, "sampler_ui: Host does not support urid:Map\n");
		free(ui);
		return NULL;
	}

	map_sampler_uris(ui->map, &ui->uris);

	lv2_atom_forge_init(&ui->forge, ui->map);

	ui->box = gtk_vbox_new(FALSE, 4);
	ui->label = gtk_label_new("?");
	ui->button = gtk_button_new_with_label("Load Sample");
	gtk_box_pack_start(GTK_BOX(ui->box), ui->label, TRUE, TRUE, 4);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->button, FALSE, FALSE, 4);
	g_signal_connect(ui->button, "clicked",
	                 G_CALLBACK(on_load_clicked),
	                 ui);
	ui->button2 = gtk_button_new_with_label("Trigger");
	gtk_box_pack_start(GTK_BOX(ui->box), ui->label, TRUE, TRUE, 4);
	gtk_box_pack_start(GTK_BOX(ui->box), ui->button2, FALSE, FALSE, 4);
	g_signal_connect(ui->button2, "clicked",
	                 G_CALLBACK(on_trig_clicked),
	                 ui);

	*widget = ui->box;

	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	SamplerUI* ui = (SamplerUI*)handle;
	gtk_widget_destroy(ui->button);
	free(ui);
}

static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
	SamplerUI* ui = (SamplerUI*)handle;
	if (format == ui->uris.atom_eventTransfer) {
		LV2_Atom* atom = (LV2_Atom*)buffer;
		if (atom->type == ui->uris.atom_Blank) {
			LV2_Atom_Object* obj      = (LV2_Atom_Object*)atom;
			const LV2_Atom*  file_uri = read_set_file(&ui->uris, obj);
			if (!file_uri) {
				fprintf(stderr, "Unknown message sent to UI.\n");
				return;
			}

			const char* uri = (const char*)LV2_ATOM_BODY(file_uri);
			gtk_label_set_text(GTK_LABEL(ui->label), uri);
		} else {
			fprintf(stderr, "Unknown message type.\n");
		}
	} else {
		fprintf(stderr, "Unknown format.\n");
	}
}

static const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2UI_Descriptor descriptor = {
	SAMPLER_UI_URI,
	instantiate,
	cleanup,
	port_event,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor*
lv2ui_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
