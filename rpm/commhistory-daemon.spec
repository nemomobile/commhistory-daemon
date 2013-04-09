Name:       commhistory-daemon
Summary:    Communications event history database daemon
Version:    0.4.7
Release:    2
Group:      Communications/Telephony and IM
License:    LGPL
URL:        https://github.com/nemomobile/commhistory-daemon
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(QtCore) >= 4.7.0
BuildRequires:  pkgconfig(QtContacts)
BuildRequires:  pkgconfig(commhistory)
BuildRequires:  pkgconfig(contextsubscriber-1.0)
BuildRequires:  pkgconfig(TelepathyQt4)
BuildRequires:  pkgconfig(mlite)
BuildRequires:  pkgconfig(mlocale)
BuildRequires:  qmsystem-devel
BuildRequires:  python

Obsoletes: smshistory <= 0.1.8
Provides: smshistory > 0.1.8
Obsoletes: voicecallhistory <= 0.1.5
Provides: voicecallhistory > 0.1.5

%package tests
Summary: Unit tests for commhistory-daemon
Group: Development/Libraries

%description tests
Unit tests for commhistory-daemon

%package ts-devel
Summary: Translation source for commhistory-daemon
Group: System/Applications

%description ts-devel
Translation source for commhistory-daemon

%description
Daemon for logging communications (IM, SMS and call) in history database.

%prep
%setup -q -n %{name}-%{version}

%build
unset LD_AS_NEEDED
%qmake 
make %{?jobs:-j%jobs}
%install
rm -rf %{buildroot}
%qmake_install

%files
%defattr(-,root,root,-)
%{_bindir}/commhistoryd
%{_datadir}/dbus-1/services/com.nokia.CommHistory.service
%{_datadir}/dbus-1/services/org.freedesktop.Telepathy.Client.CommHistory.service
%{_datadir}/dbus-1/services/org.nemomobile.AccountPresence.service
%{_datadir}/translations/commhistoryd.qm
%{_datadir}/lipstick/notificationcategories/*
%{_datadir}/telepathy/clients/CommHistory.client

%files tests
%defattr(-,root,root,-)
/opt/tests/commhistory-daemon/*

%files ts-devel
%defattr(-,root,root,-)
%{_datadir}/translations/source/commhistoryd.ts

