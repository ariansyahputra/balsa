/* -*-mode:c; c-style:k&r; c-basic-offset:4; -*- */
/* Balsa E-Mail Client
 * Copyright (C) 1997-2019 Stuart Parmenter and others,
 *                         See the file AUTHORS for a list.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option) 
 * any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H
# include "config.h"
#endif                          /* HAVE_CONFIG_H */

#include <string.h>
#include <gtk/gtk.h>
#include "balsa-app.h"
#include "print.h"
#include "misc.h"
#include "balsa-message.h"
#include "quote-color.h"
#include <glib/gi18n.h>
#include "balsa-print-object.h"
#include "balsa-print-object-decor.h"
#include "balsa-print-object-header.h"

#if HAVE__NL_MEASUREMENT_MEASUREMENT
#include <langinfo.h>
#endif                          /* HAVE__NL_MEASUREMENT_MEASUREMENT */

typedef struct {
    GtkWidget *header_font;
    GtkWidget *body_font;
    GtkWidget *footer_font;
    GtkWidget *highlight_cited;
    GtkWidget *highlight_phrases;
    GtkWidget *margin_top;
    GtkWidget *margin_bottom;
    GtkWidget *margin_left;
    GtkWidget *margin_right;
#ifdef HAVE_HTML_WIDGET
    GtkWidget *html_print;
    GtkWidget *html_load_imgs;
    BalsaPrintSetup *setup;
#endif
} BalsaPrintPrefs;

typedef struct {
    /* related message */
    LibBalsaMessage *message;
    
    /* print setup */
    BalsaPrintSetup setup;

    /* print data */
    GList *print_parts;
 
    /* header related stuff */
    gdouble c_header_y;

    /* page footer related stuff */
    gchar *footer;
    gdouble c_footer_y;
} BalsaPrintData;


/* print the page header and footer */
static void
print_header_footer(GtkPrintContext * context, cairo_t * cairo_ctx,
		    BalsaPrintData * pdata, gint pageno)
{
    PangoLayout *layout;
    PangoFontDescription *font;
    gchar *pagebuf;

    /* page number */
    font = pango_font_description_from_string(balsa_app.print_header_font);
    layout = gtk_print_context_create_pango_layout(context);
    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);
    pango_layout_set_width(layout, C_TO_P(pdata->setup.c_width));
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
    pagebuf =
	g_strdup_printf(_("Page %d of %d"), pageno + 1,	pdata->setup.page_count);
    pango_layout_set_text(layout, pagebuf, -1);
    g_free(pagebuf);
    cairo_move_to(cairo_ctx, pdata->setup.c_x0, pdata->c_header_y);
    pango_cairo_show_layout(cairo_ctx, layout);
    g_object_unref(G_OBJECT(layout));

    /* footer (if available) */
    if (pdata->footer) {
	font =
	    pango_font_description_from_string(balsa_app.
					       print_footer_font);
	layout = gtk_print_context_create_pango_layout(context);
	pango_layout_set_font_description(layout, font);
	pango_font_description_free(font);
	pango_layout_set_width(layout, C_TO_P(pdata->setup.c_width));
	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_text(layout, pdata->footer, -1);
	cairo_move_to(cairo_ctx, pdata->setup.c_x0, pdata->c_footer_y);
	pango_cairo_show_layout(cairo_ctx, layout);
	g_object_unref(G_OBJECT(layout));
    }
}


/*
 * Scan the parts of a multipart/alternative as to find the proper one for printing.  According to RFC 2046, Sect. 5.1.4, the first
 * /should/ be the "plain" one, and the following the "fancier" ones.  This is not guaranteed, though.
 * 
 * Furthermore, we may have cases like
 * +- multipart/alternative
 *      +- text/plain
 *      +- multipart/mixed
 *           +- text/html
 *           +- ...
 */
static LibBalsaMessageBody *
find_alt_part(LibBalsaMessageBody *parts,
			  gboolean			   print_alt_html)
{
	LibBalsaMessageBody *use_part;

	/* scan the parts */
	for (use_part = parts; use_part != NULL; use_part = use_part->next) {
		gchar *mime_type;
		
		mime_type = libbalsa_message_body_get_mime_type(use_part);
		if ((g_ascii_strncasecmp(mime_type, "multipart/", 10U) == 0) && (use_part->parts != NULL)) {
			/* consider the first child of a multipart */
			g_free(mime_type);
			mime_type = libbalsa_message_body_get_mime_type(use_part->parts);
		}

		if (((g_ascii_strcasecmp(mime_type, "text/plain") == 0) && !print_alt_html) ||
			((g_ascii_strcasecmp(mime_type, "text/html") == 0) && print_alt_html)) {
			g_free(mime_type);
			return use_part;
		}
		g_free(mime_type);
	}
	
	/* nothing found, fall back to the first part in the chain */
	return parts;
}


static GList *
print_single_part(GList 			  *bpo_list,
				  GtkPrintContext     *context,
				  BalsaPrintSetup     *psetup,
				  LibBalsaMessageBody *body,
				  gboolean 			   no_first_sep,
				  gboolean			   add_signature)
{
	if (!no_first_sep) {
		bpo_list = balsa_print_object_separator(bpo_list, psetup);
	}
	if (add_signature) {
#ifdef HAVE_GPGME
		if (body->was_encrypted) {
			bpo_list = balsa_print_object_frame_begin(bpo_list, _("Signed and encrypted matter"), psetup);
		} else {
			bpo_list = balsa_print_object_frame_begin(bpo_list, _("Signed matter"), psetup);
		}
#endif				/* HAVE_GPGME */
	}
	return balsa_print_objects_append_from_body(bpo_list, context, body, psetup);
}


/*
 * scan the body list and prepare print data according to the content type
 */
static GList *
scan_body(GList *bpo_list, GtkPrintContext * context, BalsaPrintSetup * psetup,
	  LibBalsaMessageBody * body, gboolean no_first_sep)
{
    gboolean add_signature = FALSE;
#ifdef HAVE_GPGME
    gboolean have_crypto_frame;
#endif				/* HAVE_GPGME */
    while (body) {
	gchar *conttype;

	conttype = libbalsa_message_body_get_mime_type(body);
#ifdef HAVE_GPGME
	add_signature = body->sig_info &&
	    g_ascii_strcasecmp(conttype, "application/pgp-signature") &&
	    g_ascii_strcasecmp(conttype, "application/pkcs7-signature") &&
	    g_ascii_strcasecmp(conttype, "application/x-pkcs7-signature");
	if (!g_ascii_strcasecmp("multipart/signed", conttype) &&
	    body->parts && body->parts->next
	    && body->parts->next->sig_info) {
	    have_crypto_frame = TRUE;
	    bpo_list = balsa_print_object_separator(bpo_list, psetup);
	    no_first_sep = TRUE;
	    if (body->was_encrypted)
		bpo_list = balsa_print_object_frame_begin(bpo_list,
							  _("Signed and encrypted matter"),
							  psetup);
	    else
		bpo_list = balsa_print_object_frame_begin(bpo_list,
							  _("Signed matter"),
							  psetup);
	} else if (!add_signature && body->was_encrypted) {
	    have_crypto_frame = TRUE;
	    bpo_list = balsa_print_object_separator(bpo_list, psetup);
	    no_first_sep = TRUE;
	    bpo_list = balsa_print_object_frame_begin(bpo_list,
						      _("Encrypted matter"),
						      psetup);
	} else
	    have_crypto_frame = FALSE;
#endif				/* HAVE_GPGME */

	if (g_ascii_strncasecmp(conttype, "multipart/", 10)) {
		bpo_list = print_single_part(bpo_list, context, psetup, body, no_first_sep, add_signature);
		no_first_sep = FALSE;
	}

	if (body->parts) {
		if (g_ascii_strcasecmp(conttype, "multipart/alternative") == 0) {
			LibBalsaMessageBody *print_part;

			print_part = find_alt_part(body->parts, psetup->print_alt_html);
			bpo_list = print_single_part(bpo_list, context, psetup, print_part, no_first_sep, add_signature);
		} else {
			bpo_list = scan_body(bpo_list, context, psetup, body->parts, no_first_sep);
		}
		no_first_sep = FALSE;
	}

	/* end the frame for an embedded message or encrypted stuff */
	if (!g_ascii_strcasecmp(conttype, "message/rfc822")
#ifdef HAVE_GPGME
	    || have_crypto_frame
#endif
	    )
	    bpo_list = balsa_print_object_frame_end(bpo_list, psetup);

#ifdef HAVE_GPGME
	if (add_signature) {
	    gchar *header =
		g_strdup_printf(_("This is an inline %s signed %s message part:"),
				body->sig_info->protocol == GPGME_PROTOCOL_OpenPGP ?
				_("OpenPGP") : _("S/MIME"), conttype);
	    bpo_list = balsa_print_object_separator(bpo_list, psetup);
	    bpo_list = balsa_print_object_header_crypto(bpo_list, context, body, header, psetup);
	    g_free(header);
	    bpo_list = balsa_print_object_frame_end(bpo_list, psetup);
	}
#endif				/* HAVE_GPGME */
	g_free(conttype);

	body = body->next;
    }

    return bpo_list;
}


static void
begin_print(GtkPrintOperation * operation, GtkPrintContext * context,
	    BalsaPrintData * pdata)
{
    GtkPageSetup *page_setup;
    PangoLayout *layout;
    PangoFontDescription *font;
    gchar *pagebuf;
    gchar *subject;
    gchar *date;
    GString *footer_string;

    /* initialise the context */
    page_setup = gtk_print_context_get_page_setup(context);

    /* calculate the "real" margins */
    pdata->setup.c_x0 = balsa_app.margin_left -
	gtk_page_setup_get_left_margin(page_setup, GTK_UNIT_POINTS);
    pdata->setup.c_width =
	gtk_page_setup_get_page_width(page_setup, GTK_UNIT_POINTS) -
	pdata->setup.c_x0 -(balsa_app.margin_right -
			    gtk_page_setup_get_right_margin(page_setup, GTK_UNIT_POINTS));
    pdata->setup.c_y0 = balsa_app.margin_top -
	gtk_page_setup_get_top_margin(page_setup, GTK_UNIT_POINTS);
    pdata->setup.c_height =
	gtk_page_setup_get_page_height(page_setup, GTK_UNIT_POINTS) -
	pdata->setup.c_y0 -(balsa_app.margin_bottom -
			    gtk_page_setup_get_bottom_margin(page_setup, GTK_UNIT_POINTS));

    pdata->setup.page_count = 1;

    /* create a layout so we can do some calculations */
    layout = gtk_print_context_create_pango_layout(context);
    pagebuf = g_strdup_printf(_("Page %d of %d"), 17, 42);

    /* basic body font height */
    font = pango_font_description_from_string(balsa_app.print_body_font);
    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);
    pdata->setup.p_body_font_height =
	p_string_height_from_layout(layout, pagebuf);

    /* basic header font and header height */
    font = pango_font_description_from_string(balsa_app.print_header_font);
    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);
    pdata->setup.p_hdr_font_height =
	p_string_height_from_layout(layout, pagebuf);
    g_free(pagebuf);

    pdata->c_header_y = pdata->setup.c_y0;
    pdata->setup.c_y0 += P_TO_C(pdata->setup.p_hdr_font_height) + C_HEADER_SEP;
    pdata->setup.c_y_pos = pdata->setup.c_y0;
    pdata->setup.c_height -=
	P_TO_C(pdata->setup.p_hdr_font_height) + C_HEADER_SEP;

    /* now create the footer string so we can reduce the height accordingly */
    subject = g_strdup(LIBBALSA_MESSAGE_GET_SUBJECT(pdata->message));
    libbalsa_utf8_sanitize(&subject, balsa_app.convert_unknown_8bit, NULL);
    if (subject)
	footer_string = g_string_new(subject);
    else
	footer_string = NULL;

    date = libbalsa_message_date_to_utf8(pdata->message, balsa_app.date_string);
    if (footer_string) {
	footer_string = g_string_append(footer_string, " \342\200\224 ");
	footer_string = g_string_append(footer_string, date);
    } else
	footer_string = g_string_new(date);
    g_free(date);

    if (pdata->message->headers->from) {
	gchar *from =
	    internet_address_list_to_string(pdata->message->headers->from, FALSE);

	libbalsa_utf8_sanitize(&from, balsa_app.convert_unknown_8bit,
			       NULL);
	if (footer_string) {
	    footer_string =
		g_string_prepend(footer_string, " \342\200\224 ");
	    footer_string = g_string_prepend(footer_string, from);
	} else {
	    footer_string = g_string_new(from);
	}
	g_free(from);
    }

    /* if a footer is available, remember the string and adjust the height */
    if (footer_string) {
	gint p_height;

	/* create a layout to calculate the height of the footer */
	font = pango_font_description_from_string(balsa_app.print_footer_font);
	pango_layout_set_font_description(layout, font);
	pango_font_description_free(font);
	pango_layout_set_width(layout, C_TO_P(pdata->setup.c_width));
	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

	/* calculate the height and adjust the pringrid region */
	p_height = p_string_height_from_layout(layout, footer_string->str);
	pdata->c_footer_y =
	    pdata->setup.c_y0 + pdata->setup.c_height - P_TO_C(p_height);
	pdata->setup.c_height -= P_TO_C(p_height) + C_HEADER_SEP;

	/* remember in the context */
	pdata->footer = g_string_free(footer_string, FALSE);
    }
    g_object_unref(G_OBJECT(layout));

    /* add the message headers */
    pdata->setup.c_y_pos = 0.0;	/* to simplify calculating the layout... */
    pdata->print_parts = 
	balsa_print_object_header_from_message(NULL, context, pdata->message,
					       subject, &pdata->setup);
    g_free(subject);

    /* add the mime bodies */
    pdata->print_parts = 
	scan_body(pdata->print_parts, context, &pdata->setup,
		  pdata->message->body_list, FALSE);

    /* done */
    gtk_print_operation_set_n_pages(operation, pdata->setup.page_count);
}


static void
draw_page(GtkPrintOperation * operation, GtkPrintContext * context,
	  gint page_nr, BalsaPrintData * print_data)
{
    cairo_t *cairo_ctx;
    GList * p;

    /* emit a warning if we try to print a non-existing page */
    if (page_nr >= print_data->setup.page_count) {
	balsa_information(LIBBALSA_INFORMATION_WARNING,
			  ngettext("Cannot print page %d because "
                                   "the document has only %d page.",
			           "Cannot print page %d because "
                                   "the document has only %d pages.",
                                   print_data->setup.page_count),
			  page_nr + 1, print_data->setup.page_count);
	return;
    }

    /* get the cairo context */
    cairo_ctx = gtk_print_context_get_cairo_context(context);

    /* print the page header and footer */
    print_header_footer(context, cairo_ctx, print_data, page_nr);

    /* print parts */
    p = print_data->print_parts;
    while (p) {
	BalsaPrintObject *po = BALSA_PRINT_OBJECT(p->data);

	if (po->on_page == page_nr)
	    balsa_print_object_draw(po, context, cairo_ctx);

	p = g_list_next(p);
    }
}

/* setup gui related stuff */
/* shamelessly stolen from gtk+-2.10.6/gtk/gtkpagesetupunixdialog.c */
static GtkUnit
get_default_user_units(void)
{
    /* Translate to the default units to use for presenting
     * lengths to the user. Translate to default:inch if you
     * want inches, otherwise translate to default:mm.
     * Do *not* translate it to "predefinito:mm", if it
     * it isn't default:mm or default:inch it will not work 
     */
    gchar *e = _("default:mm");
  
#if HAVE__NL_MEASUREMENT_MEASUREMENT
    gchar *imperial = NULL;
  
    imperial = nl_langinfo(_NL_MEASUREMENT_MEASUREMENT);
    if (imperial && imperial[0] == 2 )
	return GTK_UNIT_INCH;  /* imperial */
    if (imperial && imperial[0] == 1 )
	return GTK_UNIT_MM;  /* metric */
#endif                          /* HAVE__NL_MEASUREMENT_MEASUREMENT */
  
    if (strcmp(e, "default:inch")==0)
	return GTK_UNIT_INCH;
    else if (strcmp(e, "default:mm"))
	g_warning("Whoever translated default:mm did so wrongly.\n");
    return GTK_UNIT_MM;
}

static GtkWidget *
add_font_button(const gchar * text, const gchar * font, GtkGrid * grid,
		gint row)
{
    GtkWidget *label;
    GtkWidget *font_button;

    label = gtk_label_new_with_mnemonic(text);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(grid, label, 1, row, 1, 1);

    font_button = gtk_font_button_new_with_font(font);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), font_button);
    gtk_widget_set_hexpand(font_button, TRUE);
    gtk_grid_attach(grid, font_button, 2, row, 1, 1);

    return font_button;
}

/* note: min and max are passed in points = 1/72" */
static GtkWidget *
add_margin_spinbtn(const gchar * text, gdouble min, gdouble max, gdouble dflt,
		   GtkGrid * grid, gint row)
{
    GtkWidget *label;
    GtkWidget *spinbtn;
    const gchar *unit;

    label = gtk_label_new_with_mnemonic(text);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(grid, label, 1, row, 1, 1);

    if (get_default_user_units() == GTK_UNIT_INCH) {
	unit = _("inch");
	spinbtn = gtk_spin_button_new_with_range(min / 72.0,
						 max / 72.0, 0.01);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn), dflt / 72.0);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spinbtn), 2);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spinbtn), 0.1, 1.0);
    } else {
	unit = _("mm");
	spinbtn = gtk_spin_button_new_with_range(min / 72.0 * 25.4,
						 max / 72.0 * 25.4, 0.1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn), dflt / 72.0 * 25.4);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spinbtn), 1);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spinbtn), 1.0, 10.0);
    }
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinbtn), TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spinbtn);
    gtk_grid_attach(grid, spinbtn, 2, row, 1, 1);

    label = gtk_label_new(unit);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(grid, label, 3, row, 1, 1);

    return spinbtn;
}

static void
check_margins(GtkAdjustment * adjustment, GtkAdjustment * other)
{
    if (gtk_adjustment_get_value(adjustment) +
        gtk_adjustment_get_value(other) >
        gtk_adjustment_get_upper(adjustment))
        gtk_adjustment_set_value(adjustment,
                                 gtk_adjustment_get_upper(adjustment) -
                                 gtk_adjustment_get_value(other));
}

static GtkWidget *
create_options_group(const gchar *label_str, GtkWidget *parent_grid, gint parent_col, gint parent_row, gint parent_width)
{
	GtkWidget *group;
	GtkWidget *label;
    GtkWidget *grid;
    gchar *markup;

    group = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_grid_attach(GTK_GRID(parent_grid), group, parent_col, parent_row, parent_width, 1);

    label = gtk_label_new(NULL);
    markup = g_strdup_printf("<b>%s</b>", label_str);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(group), label, FALSE, FALSE, 0);

	grid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(group), grid, FALSE, FALSE, 0);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 0);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("    "), 0, 0, 1, 1);
	return grid;
}

static GtkWidget *
message_prefs_widget(GtkPrintOperation * operation,
		     BalsaPrintPrefs * print_prefs)
{
    GtkWidget *page;
    GtkWidget *grid;
#ifdef HAVE_HTML_WIDGET
    GtkWidget *dummy;
#endif
    GtkPageSetup *pg_setup;

    gtk_print_operation_set_custom_tab_label(operation, _("Message"));

    page = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(page), 18);
    gtk_grid_set_row_spacing(GTK_GRID(page), 18);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    /* fonts */
    grid = create_options_group(_("Fonts"), page, 0, 0, 3);

    print_prefs->header_font = add_font_button(_("_Header Font:"), balsa_app.print_header_font, GTK_GRID(grid), 0);
    print_prefs->body_font = add_font_button(_("B_ody Font:"), balsa_app.print_body_font, GTK_GRID(grid), 1);
    print_prefs->footer_font = add_font_button(_("_Footer Font:"), balsa_app.print_footer_font, GTK_GRID(grid), 2);

    /* syntax highlighting */
    grid = create_options_group(_("Highlighting"), page, 0, 1, 1);

    print_prefs->highlight_cited =
	gtk_check_button_new_with_mnemonic(_("Highlight _cited text"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(print_prefs->highlight_cited), balsa_app.print_highlight_cited);
    gtk_grid_attach(GTK_GRID(grid), print_prefs->highlight_cited, 1, 0, 1, 1);

    print_prefs->highlight_phrases = gtk_check_button_new_with_mnemonic(_("Highlight _structured phrases"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(print_prefs->highlight_phrases), balsa_app.print_highlight_phrases);
    gtk_grid_attach(GTK_GRID(grid), print_prefs->highlight_phrases, 1, 1, 1, 1);

#ifdef HAVE_HTML_WIDGET
    /* treatment of HTML messages and parts */
    grid = create_options_group(_("Highlighting"), page, 1, 1, 1);

    print_prefs->html_print = gtk_check_button_new_with_mnemonic(_("Prefer text/plain over HTML"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(print_prefs->html_print), balsa_app.display_alt_plain);
    gtk_grid_attach(GTK_GRID(grid), print_prefs->html_print, 1, 0, 1, 1);

    print_prefs->html_load_imgs = gtk_check_button_new_with_mnemonic(_("Download images from remote servers (may be dangerous)"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(print_prefs->html_load_imgs), FALSE);
    gtk_grid_attach(GTK_GRID(grid), print_prefs->html_load_imgs, 1, 1, 1, 1);

    /* phantom alignment */
    dummy = gtk_label_new(" ");
    gtk_widget_set_hexpand(dummy, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dummy, 2, 1, 1, 1);
#endif

    /* margins */
    grid = create_options_group(_("Margins"), page, 0, 2, 2);

    pg_setup = gtk_print_operation_get_default_page_setup(operation);
    print_prefs->margin_top =
	add_margin_spinbtn(_("_Top"),
			   gtk_page_setup_get_top_margin(pg_setup, GTK_UNIT_POINTS),
			   gtk_page_setup_get_page_height(pg_setup, GTK_UNIT_POINTS),
			   balsa_app.margin_top,
			   GTK_GRID(grid), 0);
    print_prefs->margin_bottom =
	add_margin_spinbtn(_("_Bottom"),
			   gtk_page_setup_get_bottom_margin(pg_setup, GTK_UNIT_POINTS),
			   gtk_page_setup_get_page_height(pg_setup, GTK_UNIT_POINTS), 
			   balsa_app.margin_bottom,
			   GTK_GRID(grid), 1);
    g_signal_connect(G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(print_prefs->margin_top))),
		     "value-changed", G_CALLBACK(check_margins),
		     gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(print_prefs->margin_bottom)));
    g_signal_connect(G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(print_prefs->margin_bottom))),
		     "value-changed", G_CALLBACK(check_margins),
		     gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(print_prefs->margin_top)));
    print_prefs->margin_left =
	add_margin_spinbtn(_("_Left"),
			   gtk_page_setup_get_left_margin(pg_setup, GTK_UNIT_POINTS),
			   gtk_page_setup_get_page_width(pg_setup, GTK_UNIT_POINTS), 
			   balsa_app.margin_left,
			   GTK_GRID(grid), 2);
    print_prefs->margin_right =
	add_margin_spinbtn(_("_Right"),
			   gtk_page_setup_get_right_margin(pg_setup, GTK_UNIT_POINTS),
			   gtk_page_setup_get_page_width(pg_setup, GTK_UNIT_POINTS), 
			   balsa_app.margin_right,
			   GTK_GRID(grid), 3);
    g_signal_connect(G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(print_prefs->margin_left))),
		     "value-changed", G_CALLBACK(check_margins),
		     gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(print_prefs->margin_right)));
    g_signal_connect(G_OBJECT(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(print_prefs->margin_right))),
		     "value-changed", G_CALLBACK(check_margins),
		     gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(print_prefs->margin_left)));

    gtk_widget_show_all(page);

    return page;
}


static void
message_prefs_apply(GtkPrintOperation * operation, GtkWidget * widget,
		    BalsaPrintPrefs * print_prefs)
{
    g_free(balsa_app.print_header_font);
    balsa_app.print_header_font =
	gtk_font_chooser_get_font(GTK_FONT_CHOOSER(print_prefs->header_font));
    g_free(balsa_app.print_body_font);
    balsa_app.print_body_font =
	gtk_font_chooser_get_font(GTK_FONT_CHOOSER(print_prefs->body_font));
    g_free(balsa_app.print_footer_font);
    balsa_app.print_footer_font =
	gtk_font_chooser_get_font(GTK_FONT_CHOOSER(print_prefs->footer_font));
    balsa_app.print_highlight_cited =
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
                                     (print_prefs->highlight_cited));
    balsa_app.print_highlight_phrases =
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
                                     (print_prefs->highlight_phrases));

    balsa_app.margin_top =
	gtk_spin_button_get_value(GTK_SPIN_BUTTON(print_prefs->margin_top)) * 72.0;
    balsa_app.margin_bottom =
	gtk_spin_button_get_value(GTK_SPIN_BUTTON(print_prefs->margin_bottom)) * 72.0;
    balsa_app.margin_left =
	gtk_spin_button_get_value(GTK_SPIN_BUTTON(print_prefs->margin_left)) * 72.0;
    balsa_app.margin_right =
	gtk_spin_button_get_value(GTK_SPIN_BUTTON(print_prefs->margin_right)) * 72.0;
    if (get_default_user_units() != GTK_UNIT_INCH) {
	/* adjust for mm */
	balsa_app.margin_top /= 25.4;
	balsa_app.margin_bottom /= 25.4;
	balsa_app.margin_left /= 25.4;
	balsa_app.margin_right /= 25.4;
    }
#ifdef HAVE_HTML_WIDGET
    print_prefs->setup->print_alt_html =
    	!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(print_prefs->html_print));
    print_prefs->setup->html_load_images =
    	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(print_prefs->html_load_imgs));
#endif
}


void
message_print_page_setup(GtkWindow * parent)
{
    GtkPageSetup *new_page_setup;

    if (!balsa_app.print_settings)
	balsa_app.print_settings = gtk_print_settings_new();

    new_page_setup =
	gtk_print_run_page_setup_dialog(parent, balsa_app.page_setup,
					balsa_app.print_settings);

    if (balsa_app.page_setup)
	g_object_unref(G_OBJECT(balsa_app.page_setup));

    balsa_app.page_setup = new_page_setup;
}


void
message_print(LibBalsaMessage * msg, GtkWindow * parent)
{
    GtkPrintOperation *print;
    GtkPrintOperationResult res;
    BalsaPrintData *print_data;
    BalsaPrintPrefs print_prefs;
    GError *err = NULL;

    print = gtk_print_operation_new();
    g_assert(print != NULL);

    g_object_ref(G_OBJECT(msg));

    gtk_print_operation_set_n_pages(print, 1);
    gtk_print_operation_set_unit(print, GTK_UNIT_POINTS);
    gtk_print_operation_set_use_full_page(print, FALSE);

    if (balsa_app.print_settings != NULL)
	gtk_print_operation_set_print_settings(print,
					       balsa_app.print_settings);
    if (balsa_app.page_setup != NULL)
	gtk_print_operation_set_default_page_setup(print,
						   balsa_app.page_setup);

    /* create a print context */
    print_data = g_new0(BalsaPrintData, 1);
    print_data->message = msg;
#ifdef HAVE_HTML_WIDGET
    print_prefs.setup = &print_data->setup;
#endif

    g_signal_connect(print, "begin_print", G_CALLBACK(begin_print), print_data);
    g_signal_connect(print, "draw_page", G_CALLBACK(draw_page), print_data);
    g_signal_connect(print, "create-custom-widget",
		     G_CALLBACK(message_prefs_widget), &print_prefs);
    g_signal_connect(print, "custom-widget-apply",
		     G_CALLBACK(message_prefs_apply), &print_prefs);

    res =
	gtk_print_operation_run(print,
				GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				parent, &err);

    if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
	if (balsa_app.print_settings != NULL)
	    g_object_unref(balsa_app.print_settings);
	balsa_app.print_settings =
	    g_object_ref(gtk_print_operation_get_print_settings(print));
    } else if (res == GTK_PRINT_OPERATION_RESULT_ERROR)
	balsa_information(LIBBALSA_INFORMATION_ERROR,
			  _("Error printing message: %s"), err->message);

    /* clean up */
    if (err)
	g_error_free(err);
    g_list_foreach(print_data->print_parts, (GFunc) g_object_unref, NULL);
    g_list_free(print_data->print_parts);
    g_free(print_data->footer);
    g_free(print_data);
    g_object_unref(G_OBJECT(print));
    g_object_unref(G_OBJECT(msg));
}
