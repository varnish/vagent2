nobase_noinst_HEADERS = common.h helpers.h plugins.h ipc.h http.h vss-hack.h vcs_version.h
BUILT_SOURCES = vcs_version.h
MAINTAINERCLEANFILES = vcs_version.h
vcs_version.h: FORCE
	if [ -d "$(top_srcdir)/.git" ]; then \
		V="$$(git describe --always --dirty)" \
		H="$$(head -n 1 vcs_version.h 2>/dev/null || true)"; \
		echo "V: $$V"; \
		if [ "/* $$V */" != "$$H" ]; then \
		( \
		  echo "/* $$V */" ;\
		  echo '/*' ;\
		  echo ' * NB:  This file is machine generated, DO NOT EDIT!' ;\
		  echo ' *' ;\
		  echo ' * Run make to regenerate' ;\
		  echo ' *'  ;\
		  echo ' */' ;\
		  echo "/* $$V */" ;\
		  echo '' ;\
		  echo "#define VCS_Version \"$$V\"" \
		) > vcs_version.h ; \
		fi \
	else \
		if [ ! -f vcs_version.h ]; then \
		( \
		  echo "/* NOGIT */" ; \
		  echo '/* No git commit ID available, see include/Makefile.am for explanation */' ; \
		  echo '#define VCS_Version "NOGIT"' \
		) >  vcs_version.h ; \
		fi \
	fi
FORCE:
