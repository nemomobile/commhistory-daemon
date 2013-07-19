Name:       commhistory-daemon
Summary:    Communications event history database daemon
Version:    0.4.11
Release:    2
Group:      Communications/Telephony and IM
License:    LGPL
URL:        https://github.com/nemomobile/commhistory-daemon
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(Qt5Versit)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(commhistory-qt5)
BuildRequires:  pkgconfig(TelepathyQt5)
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(mlocale5)
BuildRequires:  pkgconfig(qmsystem2-qt5)
BuildRequires:  qt5-qttools
BuildRequires:  qt5-qttools-linguist
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
%qmake5
make %{?jobs:-j%jobs}
%install
rm -rf %{buildroot}
%qmake5_install

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

