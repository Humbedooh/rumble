bin_PROGRAMS = rumble greylist.so starttls.so spamassassin.so whitelist.so blacklist.so foreman.so
rumble_SOURCES = src/comm.c src/hooks.c src/modules.c src/radb.c src/smtp_server.c src/config.c src/imap_server.c src/mx.c src/reply_codes.c src/smtp_worker.c src/crypt.c src/init_servers.c src/pop3_server.c src/rumble_lua.c src/cvector.c src/mailman.c src/private.c src/database.c src/main.c src/public.c src/services.c
rumble_LDFLAGS = -fPIC @LDFLAGS@ @GNUTLS_LIBS@ @LUA_LIBS@ @SQLITE_LIBS@ @MYSQL_LIBS@ -lgcrypt
rumble_CFLAGS = -fPIC @CFLAGS@

instdir = /usr/lib
inst_LIBRARIES = librumble.a

librumble_a_SOURCES = src/comm.c src/hooks.c src/modules.c src/radb.c src/smtp_server.c src/config.c src/imap_server.c src/mx.c src/reply_codes.c src/smtp_worker.c src/crypt.c src/init_servers.c src/pop3_server.c src/rumble_lua.c src/cvector.c src/mailman.c src/private.c src/database.c src/main.c src/public.c src/services.c
librumble_a_CFLAGS = -fPIC @CFLAGS@

AUTOMAKE_OPTIONS = subdir-objects

greylist_so_SOURCES = src/modules/greylist/greylist.c
greylist_so_CFLAGS = -fPIC @CFLAGS@
greylist_so_LDFLAGS = -shared -L ./ @LDFLAGS@
greylist_so_LDADD = librumble.a

blacklist_so_SOURCES = src/modules/blacklist/blacklist.c
blacklist_so_CFLAGS = -fPIC @CFLAGS@
blacklist_so_LDFLAGS = -shared -L ./ @LDFLAGS@
blacklist_so_LDADD = librumble.a

starttls_so_SOURCES = src/modules/starttls/module.c
starttls_so_CFLAGS = -fPIC @CFLAGS@
starttls_so_LDFLAGS = -shared -L ./ @LDFLAGS@
starttls_so_LDADD = librumble.a

foreman_so_SOURCES = src/modules/foreman/module.c
foreman_so_CFLAGS = -fPIC @CFLAGS@
foreman_so_LDFLAGS = -shared -L ./ @LDFLAGS@
foreman_so_LDADD = librumble.a

spamassassin_so_SOURCES = src/modules/spamassassin/spamassassin.c
spamassassin_so_CFLAGS = -fPIC @CFLAGS@
spamassassin_so_LDFLAGS = -shared -L ./ @LDFLAGS@
spamassassin_so_LDADD = librumble.a

whitelist_so_SOURCES = src/modules/whitelist/whitelist.c
whitelist_so_CFLAGS = -fPIC @CFLAGS@
whitelist_so_LDFLAGS = -shared -L ./ @LDFLAGS@
whitelist_so_LDADD = librumble.a

foomoduledir = /usr/lib/rumble
nobase_dist_foomodule_DATA = greylist.so starttls.so spamassassin.so whitelist.so blacklist.so foreman.so
