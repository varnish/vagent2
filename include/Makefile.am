nobase_noinst_HEADERS = common.h helpers.h plugins.h ipc.h http.h vss-hack.h vagent_version.h
BUILT_SOURCES = vagent_version.h
MAINTAINERCLEANFILES = vagent_version.h
vagent_version.h: FORCE
	if [ -d "$(top_srcdir)/.git" ]; then \
		V=$$(git describe --always --tags --dirty 2>/dev/null || echo NOGIT); \
		echo "V: $$V"; \
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
			echo "#define VAGENT_VERSION \"$$V\"" \
		) > vagent_version.h ;\
	else \
		if [ ! -f vagent_version.h ]; then \
			( \
			  echo "/* NOGIT */" ; \
			  echo '/* No git commit ID available, see include/Makefile.am for explanation */' ; \
			  echo '#define VAGENT_VERSION "NOGIT"' \
			) >  vagent_version.h ; \
		fi \
	fi
FORCE:
