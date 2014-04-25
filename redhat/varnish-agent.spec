Summary: varnish-agent
Name: varnish-agent
Version: 2.2.0
Release: 1%{?dist}
License: BSD
Group: System Environment/Daemons
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: varnish >= 4.0.0
Requires: libedit-devel

%if 0%{?el5}
BuildRequires: libmicrohttpd-devel varnish-libs-devel curl-devel python-docutils varnish >= 4.0.0 perl-libwww-perl nc python-demjson libedit-devel
%else
BuildRequires: libmicrohttpd-devel varnish-libs-devel libcurl-devel python-docutils varnish >= 4.0.0 perl-libwww-perl nc python-demjson libedit-devel
%endif

%description
Varnish Agent software that runs on all caches managed by Varnish
Administration Console (VAC).

%prep
%setup

%build
./configure --prefix=/usr --localstatedir=/var/lib
make VERBOSE=1

%check
make check VERBOSE=1

%install
make install DESTDIR=%{buildroot}
install -D redhat/varnish-agent.sysconfig   %{buildroot}/etc/sysconfig/varnish-agent
install -D redhat/varnish-agent.initrc      %{buildroot}/etc/init.d/varnish-agent
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
%config(noreplace) /etc/init.d/varnish-agent
%config(noreplace) /etc/sysconfig/varnish-agent
%ghost %attr(600, -, -) /etc/varnish/agent_secret
%attr(-, varnish, varnish) /var/lib/varnish-agent

%post
test -f /etc/varnish/agent_secret || \
    (echo "varnish:$(head -c 8 /dev/urandom | base64)" > /etc/varnish/agent_secret \
    && chmod 0600 /etc/varnish/agent_secret)
/sbin/chkconfig --add varnish-agent

%preun
if [ $1 -lt 1 ]; then
/sbin/service varnish-agent stop > /dev/null 2>&1
/sbin/chkconfig --del varnish-agent
fi

%changelog
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
