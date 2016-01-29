Summary: varnish-agent
Name: varnish-agent
Version: 4.1.0
Release: 1%{?dist}
License: BSD
Group: System Environment/Daemons
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: varnish
%if 0%{?fedora} >= 17 || 0%{?rhel} >= 7
Requires(post): systemd-units
Requires(post): systemd-sysv
Requires(preun): systemd-units
Requires(postun): systemd-units
BuildRequires: systemd-units
BuildRequires: strace
%endif

%if 0%{?el5}
BuildRequires: libmicrohttpd-devel varnish-libs-devel curl-devel python-docutils varnish perl-libwww-perl nc python-demjson libedit-devel
%else
BuildRequires: libmicrohttpd-devel varnish-libs-devel libcurl-devel python-docutils varnish perl-libwww-perl nc python-demjson libedit-devel
%endif

%description
Varnish Agent is a small daemon meant to communicate with Varnish and other
varnish-related services to allow remote control and monitoring of Varnish.

Required component for running the Varnish Administration Console (VAC) from Varnish Software.

%prep
%setup

%build
./configure --prefix=/usr --localstatedir=/var/lib
make VERBOSE=1

%check
make check VERBOSE=1

%install
make install DESTDIR=%{buildroot}
%if 0%{?fedora} >= 17 || 0%{?rhel} >= 7
install -D redhat/varnish-agent.service %{buildroot}%{_unitdir}/varnish-agent.service
install -D redhat/varnish-agent.params %{buildroot}%{_sysconfdir}/varnish/varnish-agent.params
%else
install -D redhat/varnish-agent.sysconfig   %{buildroot}/etc/sysconfig/varnish-agent
install -D redhat/varnish-agent.initrc      %{buildroot}/etc/init.d/varnish-agent
%endif
mkdir -p %{buildroot}/etc/varnish
touch %{buildroot}/etc/varnish/agent_secret
mkdir -p %{buildroot}/var/lib/varnish-agent

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/varnish-agent
%{_mandir}/man1/varnish-agent.1.gz
%{_datadir}/varnish-agent/html
%if 0%{?fedora} >= 17 || 0%{?rhel} >= 7
%{_unitdir}/varnish-agent.service
%config(noreplace)%{_sysconfdir}/varnish/varnish-agent.params
%else
%config(noreplace) /etc/init.d/varnish-agent
%config(noreplace) /etc/sysconfig/varnish-agent
%endif
%ghost %attr(600, -, -) /etc/varnish/agent_secret
%attr(-, varnish, varnish) /var/lib/varnish-agent

%post
test -f /etc/varnish/agent_secret || \
    (echo "varnish:$(head -c 8 /dev/urandom | base64)" > /etc/varnish/agent_secret \
    && chmod 0600 /etc/varnish/agent_secret)
%if 0%{?fedora} >= 17 || 0%{?rhel} >= 7
/bin/systemctl daemon-reload >/dev/null 2>&1 || :
%else
/sbin/chkconfig --add varnish-agent
%endif

%preun
if [ $1 -lt 1 ]; then
%if 0%{?fedora} >= 17 || 0%{?rhel} >= 7
/bin/systemctl --no-reload disable varnish-agent.service > /dev/null 2>&1 || :
/bin/systemctl stop varnish-agent.service > /dev/null 2>&1 || :
%else
/sbin/service varnish-agent stop > /dev/null 2>&1
/sbin/chkconfig --del varnish-agent
%endif
fi

%changelog

* Tue Jan 26 2016 Hugo Cruz <hugocruz.software.developer@gmail.com> - 4.1.0
- Compatible with varnish >= 4.1.0

* Mon May 19 2014 Yves Hwang <yveshwang@gmail.com> - 4.0.0
- Compatible with varnish >= 4.0.0

* Fri Apr 25 2014 Yves Hwang <yveshwang@gmail.com> - 2.2.1
- Compatible with varnish >= 3.0.5
- #109 Do not set CURLOPT_NOBODY if we have data to send.
- #108 libcurl issues HEAD instead of PUT in rhel5
- Fix an issue related to unsafe sigalarm use in older versions of libcurl.

* Mon Oct 28 2013 Dridi Boukelmoune <dridi.boukelmoune@gmail.com> - 2.2-1
- Added /etc/varnish/agent_secret in the files list

* Sat Mar 16 2013 Patricio Bruna <pbruna@itlinux.cl> - 2.2-1
- Added dependencies for rpmbuild

* Fri Feb 22 2013 Kristian Lyngstøl <kristian@bohemians.org> - 2.2-1
- Release

* Wed Jan 30 2013 Kristian Lyngstol <kristian@bohemians.org> - 2.1-1
- 2.1 dev version

* Fri Jan 18 2013 Lasse Karstensen <lkarsten@varnish-software.com> - 2.0-1
- Initial version.
