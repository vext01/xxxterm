/*
 * Copyright (c) 2011 Conformal Systems LLC <info@conformal.com>
 * Copyright (c) 2011 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2010-2012 Edd Barrett <vext01@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ctype.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/wait.h>
#if defined(__linux__)
#include "linux/util.h"
#include "linux/tree.h"
#include <bsd/stdlib.h>
# if !defined(sane_libbsd_headers)
void		arc4random_buf(void *, size_t);
u_int32_t	arc4random_uniform(u_int32_t);
# endif
#elif defined(__FreeBSD__)
#include <libutil.h>
#include "freebsd/util.h"
#include <sys/tree.h>
#else /* OpenBSD */
#include <util.h>
#include <sys/tree.h>
#endif
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#if GTK_CHECK_VERSION(3,0,0)
/* we still use GDK_* instead of GDK_KEY_* */
#include <gdk/gdkkeysyms-compat.h>
#endif

#include <webkit/webkit.h>
#include <libsoup/soup.h>
#include <JavaScriptCore/JavaScript.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

/* comment if you don't want to use threads */
#define USE_THREADS

#ifdef USE_THREADS
#include <gcrypt.h>
#include <pthread.h>
#endif

#include "javascript.h"
/*
javascript.h borrowed from vimprobable2 under the following license:

Copyright (c) 2009 Leon Winter
Copyright (c) 2009-2011 Hannes Schueller
Copyright (c) 2009-2010 Matto Fransen
Copyright (c) 2010-2011 Hans-Peter Deifel
Copyright (c) 2010-2011 Thomas Adam
Copyright (c) 2011 Albert Kim
Copyright (c) 2011 Daniel Carl

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/*#define XT_DEBUG*/
#ifdef XT_DEBUG
#define DPRINTF(x...)		do { if (swm_debug) fprintf(stderr, x); } while (0)
#define DNPRINTF(n,x...)	do { if (swm_debug & n) fprintf(stderr, x); } while (0)
#define XT_D_MOVE		0x0001
#define XT_D_KEY		0x0002
#define XT_D_TAB		0x0004
#define XT_D_URL		0x0008
#define XT_D_CMD		0x0010
#define XT_D_NAV		0x0020
#define XT_D_DOWNLOAD		0x0040
#define XT_D_CONFIG		0x0080
#define XT_D_JS			0x0100
#define XT_D_FAVORITE		0x0200
#define XT_D_PRINTING		0x0400
#define XT_D_COOKIE		0x0800
#define XT_D_KEYBINDING		0x1000
#define XT_D_CLIP		0x2000
#define XT_D_BUFFERCMD		0x4000
#define XT_D_INSPECTOR		0x8000
#define XT_D_VISITED		0x10000
#define XT_D_HISTORY		0x20000
extern u_int32_t	swm_debug;
#else
#define DPRINTF(x...)
#define DNPRINTF(n,x...)
#endif

#define LENGTH(x)		(sizeof x / sizeof x[0])
#define CLEAN(mask)		(mask & ~(GDK_MOD2_MASK) &	\
				    ~(GDK_BUTTON1_MASK) &	\
				    ~(GDK_BUTTON2_MASK) &	\
				    ~(GDK_BUTTON3_MASK) &	\
				    ~(GDK_BUTTON4_MASK) &	\
				    ~(GDK_BUTTON5_MASK))

#define XT_NOMARKS		(('z' - 'a' + 1) * 2 + 10)

struct tab {
	TAILQ_ENTRY(tab)	entry;
	GtkWidget		*vbox;
	GtkWidget		*tab_content;
	struct {
		GtkWidget	*label;
		GtkWidget	*favicon;
		GtkWidget	*eventbox;
		GtkWidget	*box;
		GtkWidget	*sep;
	} tab_elems;
	GtkWidget		*label;
	GtkWidget		*spinner;
	GtkWidget		*uri_entry;
	GtkWidget		*search_entry;
	GtkWidget		*toolbar;
	GtkWidget		*browser_win;
	GtkWidget		*statusbar_box;
	struct {
		GtkWidget	*statusbar;
		GtkWidget	*buffercmd;
		GtkWidget	*zoom;
		GtkWidget	*position;
	} sbe;
	GtkWidget		*cmd;
	GtkWidget		*buffers;
	GtkWidget		*oops;
	GtkWidget		*backward;
	GtkWidget		*forward;
	GtkWidget		*stop;
	GtkWidget		*gohome;
	GtkWidget		*js_toggle;
	GtkEntryCompletion	*completion;
	guint			tab_id;
	WebKitWebView		*wv;

	WebKitWebHistoryItem		*item;
	WebKitWebBackForwardList	*bfl;

	/* favicon */
	WebKitNetworkRequest	*icon_request;
	WebKitDownload		*icon_download;
	gchar			*icon_dest_uri;

	/* adjustments for browser */
	GtkScrollbar		*sb_h;
	GtkScrollbar		*sb_v;
	GtkAdjustment		*adjust_h;
	GtkAdjustment		*adjust_v;

	/* flags */
	int			mode;
#define XT_MODE_COMMAND		(0)
#define XT_MODE_INSERT		(1)
#define XT_MODE_HINT		(2)
#define XT_MODE_PASSTHROUGH	(3)
	int			(*mode_cb)(struct tab *, GdkEventKey *, gpointer);
	gpointer		mode_cb_data;
	int			focus_wv;
	int			ctrl_click;
	gchar			*status;
	int			xtp_meaning; /* identifies dls/favorites */
	gchar			*tmp_uri;
	int			popup; /* 1 if cmd_entry has popup visible */
#ifdef USE_THREADS
	/* https thread stuff */
	GThread			*thread;
#endif
	/* hints */
	int			script_init;
	int			new_tab;

	/* custom stylesheet */
	int			styled;
	char			*stylesheet;

	/* search */
	char			*search_text;
	int			search_forward;
	guint			search_id;

	/* settings */
	WebKitWebSettings	*settings;
	gchar			*user_agent;
	gboolean		load_images;

	/* marks */
	double			mark[XT_NOMARKS];

	/* inspector */
	WebKitWebInspector	*inspector;
	GtkWidget		*inspector_window;
	GtkWidget		*inspector_view;
};
TAILQ_HEAD(tab_list, tab);

struct karg {
	int		i;
	char		*s;
	int		precount;
};

struct download {
	RB_ENTRY(download)	entry;
	int			id;
	WebKitDownload		*download;
	struct tab		*tab;
};
RB_HEAD(download_list, download);
RB_PROTOTYPE(download_list, download, entry, download_rb_cmp);

struct history {
	RB_ENTRY(history)	entry;
	gchar			*uri;
	gchar			*title;
	time_t			time; /* When the item was added. */
};
RB_HEAD(history_list, history);
RB_PROTOTYPE(history_list, history, entry, history_rb_cmp);

/* utility */
#define XT_NAME			("XXXTerm")
#define XT_CB_HANDLED		(TRUE)
#define XT_CB_PASSTHROUGH	(FALSE)
#define XT_FAVS_FILE		("favorites")

int			run_script(struct tab *, char *);
void			xt_icon_from_file(struct tab *, char *);
GtkWidget		*create_window(const gchar *);
void			show_oops(struct tab *, const char *, ...);
gchar			*get_html_page(gchar *, gchar *, gchar *, bool);
const gchar		*get_uri(struct tab *);
const gchar		*get_title(struct tab *, bool);
void			load_uri(struct tab *t, gchar *uri);
gboolean		match_uri(const gchar *uri, const gchar *key);
int			valid_url_type(char *);

void			load_webkit_string(struct tab *, const char *, gchar *);
void			button_set_stockid(GtkWidget *, char *);

/* cookies */
int			remove_cookie(int);
int			remove_cookie_domain(int);
int			remove_cookie_all(void);
void			print_cookie(char *msg, SoupCookie *);
void			setup_cookies(void);

/* history */
int			insert_history_item(const gchar *uri, const gchar *title, time_t time);
int			save_global_history_to_disk(struct tab *t);
int			restore_global_history(void);
char			*color_visited_helper(void);
int			color_visited(struct tab *t, char *visited);

/* completion */
void			completion_add(struct tab *);
void			completion_add_uri(const gchar *uri);

/* external editor */
#define XT_EE_BUFSZ	(64 * 1024)
int			edit_src(struct tab *t, struct karg *args);
int			edit_element(struct tab *t, struct karg *a);

/* proxy */
#define XT_PRXY_SHOW		(1<<0)
#define XT_PRXY_TOGGLE		(1<<1)

/* inspector */
#define XT_INS_SHOW		(1<<0)
#define XT_INS_HIDE		(1<<1)
#define XT_INS_CLOSE		(1<<2)

WebKitWebView*		inspector_inspect_web_view_cb(WebKitWebInspector *,
			    WebKitWebView*, struct tab *);
void			setup_inspector(struct tab *);
int			inspector_cmd(struct tab *, struct karg *);

/* tld public suffix */
void			tld_tree_init();
char			*tld_get_suffix(const char *);

/* about */
#define XT_XTP_STR		"xxxt://"
#define XT_URI_ABOUT		("about:")
#define XT_URI_ABOUT_LEN	(strlen(XT_URI_ABOUT))
#define XT_URI_ABOUT_ABOUT	("about")
#define XT_URI_ABOUT_BLANK	("blank")
#define XT_URI_ABOUT_CERTS	("certs")
#define XT_URI_ABOUT_COOKIEWL	("cookiewl")
#define XT_URI_ABOUT_COOKIEJAR	("cookiejar")
#define XT_URI_ABOUT_DOWNLOADS	("downloads")
#define XT_URI_ABOUT_FAVORITES	("favorites")
#define XT_URI_ABOUT_HELP	("help")
#define XT_URI_ABOUT_HISTORY	("history")
#define XT_URI_ABOUT_JSWL	("jswl")
#define XT_URI_ABOUT_PLUGINWL	("plwl")
#define XT_URI_ABOUT_SET	("set")
#define XT_URI_ABOUT_STATS	("stats")
#define XT_URI_ABOUT_MARCO	("marco")
#define XT_URI_ABOUT_STARTPAGE	("startpage")

struct about_type {
	char		*name;
	int		(*func)(struct tab *, struct karg *);
};

struct sp {
	char			*line;
	TAILQ_ENTRY(sp)		entry;
};
TAILQ_HEAD(sp_list, sp);

int			blank(struct tab *, struct karg *);
int			help(struct tab *, struct karg *);
int			about(struct tab *, struct karg *);
int			stats(struct tab *, struct karg *);
void			show_certs(struct tab *, gnutls_x509_crt_t *,
			    size_t, char *);
int			ca_cmd(struct tab *, struct karg *);
int			cookie_show_wl(struct tab *, struct karg *);
int			xtp_page_cl(struct tab *, struct karg *);
int			xtp_page_dl(struct tab *, struct karg *);
int			xtp_page_fl(struct tab *, struct karg *);
int			xtp_page_hl(struct tab *, struct karg *);
int			parse_xtp_url(struct tab *, const char *);
int			add_favorite(struct tab *, struct karg *);
void			update_favorite_tabs(struct tab *);
void			update_history_tabs(struct tab *);
void			update_download_tabs(struct tab *);
void			xtp_generate_keys(void);
size_t			about_list_size(void);
int			cookie_cmd(struct tab *, struct karg *);
int			js_cmd(struct tab *, struct karg *);
int			pl_cmd(struct tab *, struct karg *);
void			startpage_add(const char *, ...);

/*
 * xtp tab meanings
 * identifies which tabs have xtp pages in (corresponding to about_list indices)
 */
#define XT_XTP_TAB_MEANING_NORMAL	(-1) /* normal url */
#define XT_XTP_TAB_MEANING_BL		(1)  /* about:blank in this tab */
#define XT_XTP_TAB_MEANING_CL		(4)  /* cookie manager in this tab */
#define XT_XTP_TAB_MEANING_DL		(5)  /* download manager in this tab */
#define XT_XTP_TAB_MEANING_FL		(6)  /* favorite manager in this tab */
#define XT_XTP_TAB_MEANING_HL		(8)  /* history manager in this tab */

/* whitelists */
#define XT_WL_TOGGLE		(1<<0)
#define XT_WL_ENABLE		(1<<1)
#define XT_WL_DISABLE		(1<<2)
#define XT_WL_FQDN		(1<<3) /* default */
#define XT_WL_TOPLEVEL		(1<<4)
#define XT_WL_PERSISTENT	(1<<5)
#define XT_WL_SESSION		(1<<6)
#define XT_WL_RELOAD		(1<<7)
#define XT_SHOW			(1<<8)
#define XT_DELETE		(1<<9)
#define XT_SAVE			(1<<10)
#define XT_OPEN			(1<<11)

#define XT_WL_INVALID		(0)
#define XT_WL_JAVASCRIPT	(1)
#define XT_WL_COOKIE		(2)
#define XT_WL_PLUGIN		(3)

struct domain {
	RB_ENTRY(domain)	entry;
	gchar			*d;
	int			handy; /* app use */
};
RB_HEAD(domain_list, domain);
RB_PROTOTYPE(domain_list, domain, entry, domain_rb_cmp);

int			wl_show(struct tab *, struct karg *, char *,
			    struct domain_list *);

/* uri aliases */
struct alias {
	char			*a_name;
	char			*a_uri;
	TAILQ_ENTRY(alias)	 entry;
};
TAILQ_HEAD(alias_list, alias);

/* mime types */
struct mime_type {
	char			*mt_type;
	char			*mt_action;
	int			mt_default;
	int			mt_download;
	TAILQ_ENTRY(mime_type)	entry;
};
TAILQ_HEAD(mime_type_list, mime_type);

struct domain *	wl_find(const gchar *, struct domain_list *);
int		wl_save(struct tab *, struct karg *, int);
int		toggle_cwl(struct tab *, struct karg *);
int		toggle_js(struct tab *, struct karg *);
int		toggle_pl(struct tab *, struct karg *);

/* input autofocus */
void		input_autofocus(struct tab *);
void		input_focus_blur(struct tab *, void *);
void		*input_check_mode(struct tab *);
int		command_mode(struct tab *, struct karg *);

/* settings */
#define XT_BM_NORMAL		(0)
#define XT_BM_WHITELIST		(1)
#define XT_BM_KIOSK		(2)

#define XT_GM_CLASSIC		(0)
#define XT_GM_MINIMAL		(1)

#define XT_TABS_NORMAL		(0)
#define XT_TABS_COMPACT		(1)

#define XT_EM_HYBRID		(0)
#define XT_EM_VI		(1)

#define XT_DM_START		(0)
#define XT_DM_ASK		(1)
#define XT_DM_ADD		(2)

#define XT_REFERER_ALWAYS	(0)
#define XT_REFERER_NEVER	(1)
#define XT_REFERER_SAME_DOMAIN	(2)
#define XT_REFERER_CUSTOM	(3)

#define CTRL			GDK_CONTROL_MASK
#define MOD1			GDK_MOD1_MASK
#define SHFT			GDK_SHIFT_MASK

struct key_binding {
	char				*cmd;
	guint				mask;
	guint				use_in_entry;
	guint				key;
	TAILQ_ENTRY(key_binding)	entry;	/* in bss so no need to init */
};
TAILQ_HEAD(keybinding_list, key_binding);

struct user_agent {
	char *value;
	TAILQ_ENTRY(user_agent)	entry;
};
TAILQ_HEAD(user_agent_list, user_agent);

struct settings {
	char		*name;
	int		type;
#define XT_S_INVALID	(0)
#define XT_S_INT	(1)
#define XT_S_STR	(2)
#define XT_S_FLOAT	(3)
	uint32_t	flags;
#define XT_SF_RESTART	(1<<0)
#define XT_SF_RUNTIME	(1<<1)
	int		*ival;
	char		**sval;
	struct special	*s;
	gfloat		*fval;
	int		(*activate)(char *);
};

int		set(struct tab *, struct karg *);
size_t		get_settings_size(void);
int		settings_add(char *, char *);
void		setup_proxy(char *);
int		set_browser_mode(struct settings *, char *);
int		set_gui_mode(struct settings *, char *);
int		set_cookie_policy(struct settings *, char *);
char		*get_browser_mode(struct settings *);
char		*get_gui_mode(struct settings *);
char		*get_cookie_policy(struct settings *);
void		init_keybindings(void);
void		config_parse(char *, int);
char		*get_setting_name(int);

#define		XT_DL_START	(0)
#define		XT_DL_RESTART	(1)
int		download_start(struct tab *, struct download *, int flag);

extern int	tabless;
extern int	enable_socket;
extern int	single_instance;
extern int	fancy_bar;
extern int	browser_mode;
extern int	enable_localstorage;
extern char	*statusbar_elems;

extern int	show_tabs;
extern int	tab_style;
extern int	show_url;
extern int	show_statusbar;
extern int	ctrl_click_focus;
extern int	cookies_enabled;
extern int	read_only_cookies;
extern int	enable_scripts;
extern int	enable_plugins;
extern gfloat	default_zoom_level;
extern char	default_script[PATH_MAX];
extern int	window_height;
extern int	window_width;
extern int	window_maximize;
extern int	icon_size;
extern int	refresh_interval;
extern int	enable_plugin_whitelist;
extern int	enable_cookie_whitelist;
extern int	enable_js_whitelist;
extern int	session_timeout;
extern int	cookie_policy;
extern char	*ssl_ca_file;
extern char	*resource_dir;
extern gboolean	ssl_strict_certs;
extern int	append_next;
extern char	*home;
extern char	*search_string;
extern char	*http_proxy;
extern char	*external_editor;
extern char	download_dir[PATH_MAX];
extern int	download_mode;
extern char	runtime_settings[PATH_MAX];
extern int	allow_volatile_cookies;
extern int	color_visited_uris;
extern int	save_global_history;
extern struct user_agent	*user_agent;
extern int	save_rejected_cookies;
extern int	session_autosave;
extern int	guess_search;
extern int	dns_prefetch;
extern gint	max_connections;
extern gint	max_host_connections;
extern gint	enable_spell_checking;
extern char	*spell_check_languages;
extern int	xterm_workaround;
extern char	*url_regex;
extern int	history_autosave;
extern char	search_file[PATH_MAX];
extern char	command_file[PATH_MAX];
extern char	*encoding;
extern int	autofocus_onload;
extern int	js_autorun_enabled;
extern char	*cmd_font_name;
extern char	*oops_font_name;
extern char	*statusbar_font_name;
extern char	*tabbar_font_name;
extern int	edit_mode;
extern int	userstyle_global;
extern int	auto_load_images;
extern int	enable_autoscroll;
extern int	enable_favicon_entry;
extern int	enable_favicon_tabs;
extern int	referer_mode;
extern char	*referer_custom;

/* globals */
extern char		*version;
extern char		*icons[];
extern char		rc_fname[PATH_MAX];
extern char		work_dir[PATH_MAX];
extern char		temp_dir[PATH_MAX];
extern struct passwd	*pwd;
long long unsigned int	blocked_cookies;
extern SoupCookieJar	*s_cookiejar;
extern SoupCookieJar	*p_cookiejar;
extern SoupSession	*session;
extern GtkNotebook	*notebook;
extern GtkListStore	*completion_model;

extern void	(*_soup_cookie_jar_add_cookie)(SoupCookieJar *, SoupCookie *);

extern struct history_list	hl;
extern int			hl_purge_count;
extern struct download_list	downloads;
extern struct tab_list		tabs;
extern struct about_type	about_list[];
extern struct domain_list	c_wl;
extern struct domain_list	js_wl;
extern struct domain_list	pl_wl;
extern struct alias_list	aliases;
extern struct mime_type_list	mtl;
extern struct keybinding_list	kbl;
extern struct sp_list		spl;
extern struct user_agent_list	ua_list;
extern int			user_agent_count;

extern PangoFontDescription	*cmd_font;
extern PangoFontDescription	*oops_font;
extern PangoFontDescription	*statusbar_font;
extern PangoFontDescription	*tabbar_font;
