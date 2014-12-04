# Makefile for program source directory in GNU NLS utilities package.
# Copyright (C) 1995, 1996, 1997 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
# Copyright (C) 2004-2008 Rodney Dawes <dobey.pwns@gmail.com>
#
# This file may be copied and used freely without restrictions.  It may
# be used in projects which are not available under a GNU Public License,
# but which still want to provide support for the GNU gettext functionality.
#
# - Modified by Owen Taylor <otaylor@redhat.com> to use GETTEXT_PACKAGE
#   instead of PACKAGE and to look for po2tbl in ./ not in intl/
#
# - Modified by jacob berkman <jacob@ximian.com> to install
#   Makefile.in.in and po2tbl.sed.in for use with glib-gettextize
#
# - Modified by Rodney Dawes <dobey.pwns@gmail.com> for use with intltool
#
# We have the following line for use by intltoolize:
# INTLTOOL_MAKEFILE

GETTEXT_PACKAGE = gthumb
PACKAGE = gthumb
VERSION = 2.10.11

SHELL = /bin/sh

srcdir = .
top_srcdir = ..
top_builddir = ..


prefix = /usr/local
exec_prefix = ${prefix}
datadir = ${datarootdir}
datarootdir = ${prefix}/share
libdir = ${exec_prefix}/lib
DATADIRNAME = share
itlocaledir = $(prefix)/$(DATADIRNAME)/locale
subdir = po
install_sh = $(SHELL) /home/stewart/codes/apps/gthumb-2.10.11_svn/install-sh
# Automake >= 1.8 provides /bin/mkdir -p.
# Until it can be supposed, use the safe fallback:
mkdir_p = $(install_sh) -d

INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644

GMSGFMT = /usr/bin/msgfmt
MSGFMT = /usr/bin/msgfmt
XGETTEXT = /usr/bin/xgettext
INTLTOOL_UPDATE = $(top_builddir)/intltool-update
INTLTOOL_EXTRACT = $(top_builddir)/intltool-extract
MSGMERGE = INTLTOOL_EXTRACT=$(INTLTOOL_EXTRACT) srcdir=$(srcdir) $(INTLTOOL_UPDATE) --gettext-package $(GETTEXT_PACKAGE) --dist
GENPOT   = INTLTOOL_EXTRACT=$(INTLTOOL_EXTRACT) srcdir=$(srcdir) $(INTLTOOL_UPDATE) --gettext-package $(GETTEXT_PACKAGE) --pot

ALL_LINGUAS = 

PO_LINGUAS=$(shell if test -r $(srcdir)/LINGUAS; then grep -v "^\#" $(srcdir)/LINGUAS; fi)

USER_LINGUAS=$(shell if test -n "$(LINGUAS)"; then LLINGUAS="$(LINGUAS)"; ALINGUAS="$(ALL_LINGUAS)"; for lang in $$LLINGUAS; do if test -n "`grep ^$$lang$$ $(srcdir)/LINGUAS`" -o -n "`echo $$ALINGUAS|grep ' ?$$lang ?'`"; then printf "$$lang "; fi; done; fi)

USE_LINGUAS=$(shell if test -n "$(USER_LINGUAS)"; then LLINGUAS="$(USER_LINGUAS)"; else if test -n "$(PO_LINGUAS)"; then LLINGUAS="$(PO_LINGUAS)"; else LLINGUAS="$(ALL_LINGUAS)"; fi; fi; for lang in $$LLINGUAS; do printf "$$lang "; done)

POFILES=$(shell LINGUAS="$(USE_LINGUAS)"; for lang in $$LINGUAS; do printf "$$lang.po "; done)

DISTFILES = ChangeLog Makefile.in.in POTFILES.in $(POFILES)
EXTRA_DISTFILES = POTFILES.skip Makevars LINGUAS

POTFILES = \
	../data/albumthemes/text.h \
	../data/glade/gthumb.glade \
	../data/glade/gthumb_camera.glade \
	../data/glade/gthumb_comments.glade \
	../data/glade/gthumb_convert.glade \
	../data/glade/gthumb_crop.glade \
	../data/glade/gthumb_edit.glade \
	../data/glade/gthumb_png_exporter.glade \
	../data/glade/gthumb_preferences.glade \
	../data/glade/gthumb_print.glade \
	../data/glade/gthumb_redeye.glade \
	../data/glade/gthumb_search.glade \
	../data/glade/gthumb_tools.glade \
	../data/glade/gthumb_web_exporter.glade \
	../data/gthumb.desktop.in \
	../data/gthumb.schemas.in \
	../libgthumb/async-pixbuf-ops.c \
	../libgthumb/async-pixbuf-ops.h \
	../libgthumb/bookmarks.c \
	../libgthumb/bookmarks.h \
	../libgthumb/catalog.c \
	../libgthumb/catalog.h \
	../libgthumb/comments.c \
	../libgthumb/comments.h \
	../libgthumb/cursors.c \
	../libgthumb/cursors.h \
	../libgthumb/dlg-save-image.c \
	../libgthumb/dlg-save-image.h \
	../libgthumb/eel-canvas-rect.c \
	../libgthumb/eel-canvas-rect.h \
	../libgthumb/file-data.c \
	../libgthumb/file-data.h \
	../libgthumb/file-utils.c \
	../libgthumb/file-utils.h \
	../libgthumb/gconf-utils.c \
	../libgthumb/gconf-utils.h \
	../libgthumb/glib-utils.c \
	../libgthumb/glib-utils.h \
	../libgthumb/gstringlist.c \
	../libgthumb/gstringlist.h \
	../libgthumb/gth-file-list.c \
	../libgthumb/gth-file-list.h \
	../libgthumb/gth-file-view-list.c \
	../libgthumb/gth-file-view-list.h \
	../libgthumb/gth-file-view-thumbs.c \
	../libgthumb/gth-file-view-thumbs.h \
	../libgthumb/gth-file-view.c \
	../libgthumb/gth-file-view.h \
	../libgthumb/gth-image-list.c \
	../libgthumb/gth-image-list.h \
	../libgthumb/gth-pixbuf-op.c \
	../libgthumb/gth-pixbuf-op.h \
	../libgthumb/gth-utils.c \
	../libgthumb/gth-utils.h \
	../libgthumb/gthumb-error.c \
	../libgthumb/gthumb-error.h \
	../libgthumb/gthumb-histogram.c \
	../libgthumb/gthumb-histogram.h \
	../libgthumb/gthumb-info-bar.c \
	../libgthumb/gthumb-info-bar.h \
	../libgthumb/gthumb-init.c \
	../libgthumb/gthumb-init.h \
	../libgthumb/gthumb-module.c \
	../libgthumb/gthumb-module.h \
	../libgthumb/gthumb-slide.c \
	../libgthumb/gthumb-slide.h \
	../libgthumb/gthumb-stock.c \
	../libgthumb/gthumb-stock.h \
	../libgthumb/gtk-utils.c \
	../libgthumb/gtk-utils.h \
	../libgthumb/image-loader.c \
	../libgthumb/image-loader.h \
	../libgthumb/image-viewer.c \
	../libgthumb/image-viewer.h \
	../libgthumb/nav-window.c \
	../libgthumb/nav-window.h \
	../libgthumb/pixbuf-utils.c \
	../libgthumb/pixbuf-utils.h \
	../libgthumb/preferences.c \
	../libgthumb/preferences.h \
	../libgthumb/print-callbacks.c \
	../libgthumb/print-callbacks.h \
	../libgthumb/search.c \
	../libgthumb/search.h \
	../libgthumb/thumb-cache.c \
	../libgthumb/thumb-cache.h \
	../libgthumb/thumb-loader.c \
	../libgthumb/thumb-loader.h \
	../libgthumb/typedefs.h \
	../src/albumtheme.c \
	../src/albumtheme-private.c \
	../src/albumtheme-private.h \
	../src/bookmark-list.c \
	../src/bookmark-list.h \
	../src/catalog-list.c \
	../src/catalog-list.h \
	../src/catalog-png-exporter.c \
	../src/catalog-png-exporter.h \
	../src/catalog-web-exporter.c \
	../src/catalog-web-exporter.h \
	../src/dlg-bookmarks.c \
	../src/dlg-bookmarks.h \
	../src/dlg-brightness-contrast.c \
	../src/dlg-brightness-contrast.h \
	../src/dlg-catalog.c \
	../src/dlg-catalog.h \
	../src/dlg-categories.c \
	../src/dlg-categories.h \
	../src/dlg-change-date.c \
	../src/dlg-change-date.h \
	../src/dlg-color-balance.c \
	../src/dlg-color-balance.h \
	../src/dlg-comment.c \
	../src/dlg-comment.h \
	../src/dlg-convert.c \
	../src/dlg-convert.h \
	../src/dlg-crop.c \
	../src/dlg-crop.h \
	../src/dlg-duplicates.c \
	../src/dlg-duplicates.h \
	../src/dlg-file-utils.c \
	../src/dlg-file-utils.h \
	../src/dlg-hue-saturation.c \
	../src/dlg-hue-saturation.h \
	../src/dlg-image-prop.c \
	../src/dlg-image-prop.h \
	../src/dlg-jpegtran.c \
	../src/dlg-jpegtran.h \
	../src/dlg-open-with.c \
	../src/dlg-open-with.h \
	../src/dlg-photo-importer.c \
	../src/dlg-photo-importer.h \
	../src/dlg-png-exporter.c \
	../src/dlg-png-exporter.h \
	../src/dlg-posterize.c \
	../src/dlg-posterize.h \
	../src/dlg-preferences.c \
	../src/dlg-preferences.h \
	../src/dlg-rename-series.c \
	../src/dlg-rename-series.h \
	../src/dlg-reset-exif.c \
	../src/dlg-reset-exif.h \
	../src/dlg-scale-image.c \
	../src/dlg-scale-image.h \
	../src/dlg-scale-series.c \
	../src/dlg-scale-series.h \
	../src/dlg-search.c \
	../src/dlg-search.h \
	../src/dlg-web-exporter.c \
	../src/dlg-web-exporter.h \
	../src/dlg-write-to-cd.c \
	../src/dlg-write-to-cd.h \
	../src/gth-batch-op.c \
	../src/gth-browser-actions-callbacks.c \
	../src/gth-browser-actions-callbacks.h \
	../src/gth-browser-actions-entries.h \
	../src/gth-browser.c \
	../src/gth-browser.h \
	../src/gth-browser-ui.h \
	../src/gth-dir-list.c \
	../src/gth-dir-list.h \
	../src/gth-exif-data-viewer.c \
	../src/gth-exif-data-viewer.h \
	../src/gth-filter-bar.c \
	../src/gth-filter-bar.h \
	../src/gth-folder-selection-dialog.c \
	../src/gth-folder-selection-dialog.h \
	../src/gth-fullscreen-actions-callbacks.c \
	../src/gth-fullscreen-actions-callbacks.h \
	../src/gth-fullscreen-actions-entries.h \
	../src/gth-fullscreen.c \
	../src/gth-fullscreen.h \
	../src/gth-fullscreen-ui.h \
	../src/gth-image-selector.c \
	../src/gth-image-selector.h \
	../src/gth-location.c \
	../src/gth-location.h \
	../src/gth-monitor.c \
	../src/gth-monitor.h \
	../src/gthumb-preloader.c \
	../src/gthumb-preloader.h \
	../src/gth-viewer-actions-callbacks.c \
	../src/gth-viewer-actions-callbacks.h \
	../src/gth-viewer-actions-entries.h \
	../src/gth-viewer.c \
	../src/gth-viewer.h \
	../src/gth-viewer-ui.h \
	../src/gth-window-actions-callbacks.c \
	../src/gth-window-actions-callbacks.h \
	../src/gth-window-actions-entries.h \
	../src/gth-window.c \
	../src/gth-window.h \
	../src/gth-window-utils.c \
	../src/gth-window-utils.h \
	../src/gtkcellrendererthreestates.c \
	../src/gtkcellrendererthreestates.h \
	../src/lex.albumtheme.c \
	../src/main.c \
	../src/main.h \
	../src/rotation-utils.c \
	../src/rotation-utils.h \
	../src/totem-scrsaver.c \
	../src/totem-scrsaver.h
# This comment gets stripped out

CATALOGS=$(shell LINGUAS="$(USE_LINGUAS)"; for lang in $$LINGUAS; do printf "$$lang.gmo "; done)

.SUFFIXES:
.SUFFIXES: .po .pox .gmo .mo .msg .cat

.po.pox:
	$(MAKE) $(GETTEXT_PACKAGE).pot
	$(MSGMERGE) $< $(GETTEXT_PACKAGE).pot -o $*.pox

.po.mo:
	$(MSGFMT) -o $@ $<

.po.gmo:
	file=`echo $* | sed 's,.*/,,'`.gmo \
	  && rm -f $$file && $(GMSGFMT) -o $$file $<

.po.cat:
	sed -f ../intl/po2msg.sed < $< > $*.msg \
	  && rm -f $@ && gencat $@ $*.msg


all: all-yes

all-yes: $(CATALOGS)
all-no:

$(GETTEXT_PACKAGE).pot: $(POTFILES)
	$(GENPOT)

install: install-data
install-data: install-data-yes
install-data-no: all
install-data-yes: all
	$(mkdir_p) $(DESTDIR)$(itlocaledir)
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  dir=$(DESTDIR)$(itlocaledir)/$$lang/LC_MESSAGES; \
	  $(mkdir_p) $$dir; \
	  if test -r $$lang.gmo; then \
	    $(INSTALL_DATA) $$lang.gmo $$dir/$(GETTEXT_PACKAGE).mo; \
	    echo "installing $$lang.gmo as $$dir/$(GETTEXT_PACKAGE).mo"; \
	  else \
	    $(INSTALL_DATA) $(srcdir)/$$lang.gmo $$dir/$(GETTEXT_PACKAGE).mo; \
	    echo "installing $(srcdir)/$$lang.gmo as" \
		 "$$dir/$(GETTEXT_PACKAGE).mo"; \
	  fi; \
	  if test -r $$lang.gmo.m; then \
	    $(INSTALL_DATA) $$lang.gmo.m $$dir/$(GETTEXT_PACKAGE).mo.m; \
	    echo "installing $$lang.gmo.m as $$dir/$(GETTEXT_PACKAGE).mo.m"; \
	  else \
	    if test -r $(srcdir)/$$lang.gmo.m ; then \
	      $(INSTALL_DATA) $(srcdir)/$$lang.gmo.m \
		$$dir/$(GETTEXT_PACKAGE).mo.m; \
	      echo "installing $(srcdir)/$$lang.gmo.m as" \
		   "$$dir/$(GETTEXT_PACKAGE).mo.m"; \
	    else \
	      true; \
	    fi; \
	  fi; \
	done

# Empty stubs to satisfy archaic automake needs
dvi info tags TAGS ID:

# Define this as empty until I found a useful application.
install-exec installcheck:

uninstall:
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  rm -f $(DESTDIR)$(itlocaledir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE).mo; \
	  rm -f $(DESTDIR)$(itlocaledir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE).mo.m; \
	done

check: all $(GETTEXT_PACKAGE).pot
	rm -f missing notexist
	srcdir=$(srcdir) $(INTLTOOL_UPDATE) -m
	if [ -r missing -o -r notexist ]; then \
	  exit 1; \
	fi

mostlyclean:
	rm -f *.pox $(GETTEXT_PACKAGE).pot *.old.po cat-id-tbl.tmp
	rm -f .intltool-merge-cache

clean: mostlyclean

distclean: clean
	rm -f Makefile Makefile.in POTFILES stamp-it
	rm -f *.mo *.msg *.cat *.cat.m *.gmo

maintainer-clean: distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."
	rm -f Makefile.in.in

distdir = ../$(PACKAGE)-$(VERSION)/$(subdir)
dist distdir: $(DISTFILES)
	dists="$(DISTFILES)"; \
	extra_dists="$(EXTRA_DISTFILES)"; \
	for file in $$extra_dists; do \
	  test -f $(srcdir)/$$file && dists="$$dists $(srcdir)/$$file"; \
	done; \
	for file in $$dists; do \
	  test -f $$file || file="$(srcdir)/$$file"; \
	  ln $$file $(distdir) 2> /dev/null \
	    || cp -p $$file $(distdir); \
	done

update-po: Makefile
	$(MAKE) $(GETTEXT_PACKAGE).pot
	tmpdir=`pwd`; \
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  echo "$$lang:"; \
	  result="`$(MSGMERGE) -o $$tmpdir/$$lang.new.po $$lang`"; \
	  if $$result; then \
	    if cmp $(srcdir)/$$lang.po $$tmpdir/$$lang.new.po >/dev/null 2>&1; then \
	      rm -f $$tmpdir/$$lang.new.po; \
            else \
	      if mv -f $$tmpdir/$$lang.new.po $$lang.po; then \
	        :; \
	      else \
	        echo "msgmerge for $$lang.po failed: cannot move $$tmpdir/$$lang.new.po to $$lang.po" 1>&2; \
	        rm -f $$tmpdir/$$lang.new.po; \
	        exit 1; \
	      fi; \
	    fi; \
	  else \
	    echo "msgmerge for $$lang.gmo failed!"; \
	    rm -f $$tmpdir/$$lang.new.po; \
	  fi; \
	done

Makefile POTFILES: stamp-it
	@if test ! -f $@; then \
	  rm -f stamp-it; \
	  $(MAKE) stamp-it; \
	fi

stamp-it: Makefile.in.in $(top_builddir)/config.status POTFILES.in
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/Makefile.in CONFIG_HEADERS= CONFIG_LINKS= \
	       $(SHELL) ./config.status

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
